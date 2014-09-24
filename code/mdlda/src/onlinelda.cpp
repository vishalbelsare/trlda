#include <utility>
using std::pair;
using std::make_pair;

#include "Eigen/Core"
using Eigen::ArrayXd;
using Eigen::ArrayXXd;

#include "onlinelda.h"
#include "utils.h"

#include <iostream>
using std::cout;
using std::endl;

#include <cmath>
using std::pow;

MDLDA::OnlineLDA::Parameters::Parameters(
	InferenceMethod inferenceMethod,
	double threshold,
	int maxIterInference,
	int maxIterMD,
	double tau,
	double kappa,
	double rho,
	bool adaptive,
	int numSamples,
	int burnIn) :
	inferenceMethod(inferenceMethod),
	threshold(threshold),
	maxIterInference(maxIterInference),
	maxIterMD(maxIterMD),
	tau(tau),
	kappa(kappa),
	rho(rho),
	adaptive(adaptive),
	numSamples(numSamples),
	burnIn(burnIn)
{
}



MDLDA::OnlineLDA::OnlineLDA(
	int numWords,
	int numTopics,
	int numDocuments,
	double alpha,
	double eta) :
		mNumDocuments(numDocuments),
		mAlpha(alpha),
		mEta(eta),
		mUpdateCounter(0)
{
	mLambda = sampleGamma(numTopics, numWords, 100) / 100.;

	mAdaTau = 1000.;
	mAdaRho = 1. / mAdaTau;
	mAdaSqNorm = 1.;
	mAdaGradient = ArrayXXd::Zero(numTopics, numWords);
}



pair<ArrayXXd, ArrayXXd> MDLDA::OnlineLDA::updateVariables(
		const Documents& documents,
		const Parameters& parameters) const
{
	switch(parameters.inferenceMethod) {
		case GIBBS:
			return updateVariables(
				documents,
				sampleDirichlet(numTopics(), documents.size()),
				parameters);

		case VI:
		default:
			// initialize with random gamma
			return updateVariables(
				documents,
				sampleGamma(numTopics(), documents.size(), 100) / 100.,
				parameters);
	}
}



pair<ArrayXXd, ArrayXXd> MDLDA::OnlineLDA::updateVariables(
		const Documents& documents,
		const ArrayXXd& latents,
		const Parameters& parameters) const
{
	switch(parameters.inferenceMethod) {
		case GIBBS:
			return updateVariablesGibbs(documents, latents, parameters);

		case VI:
		default:
			return updateVariablesVI(documents, latents, parameters);

	}
}



pair<ArrayXXd, ArrayXXd> MDLDA::OnlineLDA::updateVariablesVI(
		const Documents& documents,
		const ArrayXXd& initialGamma,
		const Parameters& parameters) const
{
	if(initialGamma.rows() != numTopics() || initialGamma.cols() != documents.size())
		throw Exception("Initial gamma has wrong dimensionality.");

	ArrayXXd gamma = initialGamma;
	ArrayXXd sstats = ArrayXXd::Zero(numTopics(), numWords());

	// compute $\exp E[ \beta | \lambda ]$ and $A \exp E[ \theta \mid \gamma ]$
	ArrayXd psiSum = digamma(mLambda.rowwise().sum());
	MatrixXd expPsiLambda = (digamma(mLambda).colwise() - psiSum).exp();
	MatrixXd expPsiGamma = digamma(gamma).exp();

	#pragma omp parallel for
	for(int i = 0; i < documents.size(); ++i) {
		// select columns needed for this document
		MatrixXd expPsiLambdaDoc(numTopics(), documents[i].size());
		for(int j = 0; j < documents[i].size(); ++j)
			expPsiLambdaDoc.col(j) = expPsiLambda.col(documents[i][j].first);

		ArrayXd phiNorm = (expPsiGamma.col(i).transpose() * expPsiLambdaDoc).array() + 1e-100;

		for(int k = 0; k < parameters.maxIterInference; ++k) {
			ArrayXd lastGamma = gamma.col(i);

			// recompute gamma, represent phi implicitly
			gamma.col(i).setZero();
			for(int j = 0; j < documents[i].size(); ++j) {
				const int& wordCount = documents[i][j].second;
				gamma.col(i) += wordCount / phiNorm[j] * expPsiLambdaDoc.col(j).array();
			}
			gamma.col(i) *= expPsiGamma.array().col(i);
			gamma.col(i) += mAlpha;

			expPsiGamma.col(i) = digamma(gamma.col(i)).exp();

			phiNorm = (expPsiGamma.col(i).transpose() * expPsiLambdaDoc).array() + 1e-100;

			// test for convergence
			if((lastGamma - gamma.col(i)).abs().mean() < parameters.threshold)
				break;
		}

		// update sufficient statistics
		for(int j = 0; j < documents[i].size(); ++j) {
			const int& wordID = documents[i][j].first;
			const int& wordCount = documents[i][j].second;

			#pragma omp critical
			sstats.col(wordID) += wordCount / phiNorm[j] * expPsiGamma.array().col(i);
		}
	}

	// finish computing sufficient statistics
	sstats *= expPsiLambda.array();

	return make_pair(gamma, sstats);
}



pair<ArrayXXd, ArrayXXd> MDLDA::OnlineLDA::updateVariablesGibbs(
		const Documents& documents,
		const ArrayXXd& initialTheta,
		const Parameters& parameters) const
{
	if(initialTheta.rows() != numTopics() || initialTheta.cols() != documents.size())
		throw Exception("Initial theta has wrong dimensionality.");

	ArrayXXd sstats = ArrayXXd::Zero(numTopics(), numWords());
	ArrayXXd theta = initialTheta;
	double unit = 1. / parameters.numSamples;

	// compute $\exp E[ \beta | \lambda ]$
	ArrayXd psiSum = digamma(mLambda.rowwise().sum());
	ArrayXXd expPsiLambda = (digamma(mLambda).colwise() - psiSum).exp();

	#pragma omp parallel for
	for(int i = 0; i < documents.size(); ++i) {
		// container for topics associated with this document
		vector<vector<int> > topics(documents[i].size());

		// counts the occurrences of topics in this document (plus alpha)
		ArrayXd counts = ArrayXd::Constant(numTopics(), mAlpha);

		// initialize topics (blocked Gibbs sampling)
		for(int j = 0; j < documents[i].size(); ++j) {
			const int& wordid = documents[i][j].first;
			const int& wordcount = documents[i][j].second;

			// unnormalized distribution over topics conditioned on theta
			ArrayXd dist = expPsiLambda.col(wordid) * theta.col(j);

			// for each occurrence of the word
			for(int k = 0; k < wordcount; ++k) {
				// sample a topic
				topics[j].push_back(sampleHistogram(dist));
				counts[topics[j][k]] += 1.;
			}
		}

		for(int s = 0; s < parameters.numSamples + parameters.burnIn; ++s) {
			// update each topic once (collapsed Gibbs sampling)
			for(int j = 0; j < documents[i].size(); ++j) {
				const int& wordid = documents[i][j].first;
				const int& wordcount = documents[i][j].second;

				for(int k = 0; k < wordcount; ++k) {
					counts[topics[j][k]] -= 1.;
					topics[j][k] = sampleHistogram(expPsiLambda.col(wordid) * counts);
					counts[topics[j][k]] += 1.;
				}
			}

			if(s >= parameters.burnIn)
				// collect sufficient statistics
				for(int j = 0; j < documents[i].size(); ++j) {
					const int& wordid = documents[i][j].first;
					const int& wordcount = documents[i][j].second;

					for(int k = 0; k < wordcount; ++k)
						sstats(topics[j][k], wordid) += unit;
				}
		}

		// resample theta
		theta.col(i) = sampleDirichlet(counts);
	}

	return make_pair(theta, sstats);
}



double MDLDA::OnlineLDA::updateParameters(const Documents& documents, const Parameters& parameters) {
	if(documents.size() == 0)
		// nothing to be done
		return true;

	// choose a learning rate
	double rho = parameters.rho;
	if(rho < 0.) {
		if(parameters.adaptive) {
			rho = mAdaRho;
		} else {
			rho = pow(parameters.tau + mUpdateCounter, -parameters.kappa);
		}
	}

	ArrayXXd lambdaPrime = mLambda;
	ArrayXXd lambdaHat;

	if(parameters.maxIterMD > 0) {
		// sufficient statistics as if $\phi_{dwk}$ was 1/K
		ArrayXd wordcounts = ArrayXd::Zero(numWords());
		for(int i = 0; i < documents.size(); ++i)
			for(int j = 0; j < documents[i].size(); ++j)
				wordcounts[documents[i][j].first] += documents[i][j].second;

		// initial update to lambda to avoid local optima
		mLambda = ((1. - rho) * lambdaPrime).rowwise()
			+ rho * (mEta + static_cast<double>(mNumDocuments) / documents.size() / numTopics() * wordcounts.transpose());

		pair<ArrayXXd, ArrayXXd> results;

		// mirror descent iterations
		for(int i = 0; i < parameters.maxIterMD; ++i) {
			// compute sufficient statistics (E-step)
			if(i > 0)
				// initialize with gamma of previous iteration
				results = updateVariables(documents, results.first, parameters);
			else
				results = updateVariables(documents, parameters);
			ArrayXXd& sstats = results.second;

			// update parameters (M-step)
			lambdaHat = mEta + static_cast<double>(mNumDocuments) / documents.size() * sstats;
			mLambda = (1. - rho) * lambdaPrime + rho * lambdaHat;
		}
	} else {
		// compute sufficient statistics (E-step)
		pair<ArrayXXd, ArrayXXd> results = updateVariables(documents, parameters);
		ArrayXXd& sstats = results.second;

		// update parameters (M-step)
		lambdaHat = mEta + static_cast<double>(mNumDocuments) / documents.size() * sstats;
		mLambda = (1. - rho) * lambdaPrime + rho * lambdaHat;
	}

	if(parameters.adaptive) {
		ArrayXXd lambdaUpdate = lambdaHat - lambdaPrime;

		// compute running average of gradients and adjust learning rate
		mAdaGradient = (1. - 1. / mAdaTau) * mAdaGradient + 1. / mAdaTau * lambdaUpdate;
		mAdaSqNorm   = (1. - 1. / mAdaTau) * mAdaSqNorm   + 1. / mAdaTau * lambdaUpdate.matrix().squaredNorm();
		mAdaRho = mAdaGradient.matrix().squaredNorm() / mAdaSqNorm;
		mAdaTau = mAdaTau * (1. - mAdaRho) + 1.;
	}

	mUpdateCounter++;

	return rho;
}
