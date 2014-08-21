import unittest

from time import time
from pickle import load, dump
from tempfile import mkstemp
from random import choice
from string import ascii_letters
from numpy import corrcoef, random, abs, max, asarray, round
from mdlda.models import OnlineLDA
from onlineldavb import OnlineLDA as ReferenceLDA

class Tests(unittest.TestCase):
	def test_e_step(self):
		W = 100
		K = 20
		D = 10
		N = 100

		# generate random vocabulary
		vocab = [''.join(choice(ascii_letters) for _ in range(5 + random.randint(10))) for _ in range(W)]
		model0 = ReferenceLDA(vocab, K, D, 0.1, 0.3, 1024., 0.9)

		model1 = OnlineLDA(num_words=W, num_topics=K, num_documents=D)
		model1.alpha = model0._alpha
		model1.lambdas = model0._lambda

		# generate D random documents of length up to N
		docs1 = []
		for _ in range(D):
			docs1.append([
				(w, random.randint(10)) for w in random.permutation(W)[:1 + random.randint(N)]])
		docs0 = [zip(*doc) for doc in docs1]

		# use the same initialization of gamma
		initial_gamma = random.gamma(100., 1./100., [K, D])

		gamma0, sstats0 = model0.do_e_step(docs0, max_steps=50, gamma=initial_gamma.T)
		gamma1, sstats1 = model1.do_e_step(docs1, max_iter=50, gamma=initial_gamma)

		# make sure e-Step gives the same results
		self.assertGreater(corrcoef(gamma0.T.ravel(), gamma1.ravel())[0, 1], 0.99)
		self.assertGreater(corrcoef(sstats0.ravel(), sstats1.ravel())[0, 1], 0.99)



	def test_pickle(self):
		model0 = OnlineLDA(
			num_words=300,
			num_topics=50,
			num_documents=11110,
			alpha=random.rand(),
			tau=random.rand() * 1000.,
			kappa=random.rand(),
			eta=random.rand())

		tmp_file = mkstemp()[1]

		# save model
		with open(tmp_file, 'w') as handle:
			dump({'model': model0}, handle)

		# load model
		with open(tmp_file) as handle:
			model1 = load(handle)['model']

		# make sure parameters haven't changed
		self.assertEqual(model0.num_words, model1.num_words)
		self.assertEqual(model0.num_topics, model1.num_topics)
		self.assertEqual(model0.num_documents, model1.num_documents)
		self.assertLess(max(abs(model0.lambdas - model1.lambdas)), 1e-20)
		self.assertLess(abs(model0.alpha - model1.alpha), 1e-20)
		self.assertLess(abs(model0.tau - model1.tau), 1e-20)
		self.assertLess(abs(model0.kappa - model1.kappa), 1e-20)
		self.assertLess(abs(model0.eta - model1.eta), 1e-20)



	def test_speed(self):
		model1 = OnlineLDA(
			num_words=1000,
			num_topics=100,
			num_documents=10000,
			alpha=.1,
			tau=1024.,
			kappa=.9,
			eta=.3)

		# random vocabulary
		vocab = [''.join(choice(ascii_letters) for _ in range(5 + random.randint(10)))
			for _ in range(model1.num_words)]

		model0 = ReferenceLDA(vocab,
			D=model1.num_documents,
			K=model1.num_topics,
			alpha=model1.alpha,
			eta=model1.eta,
			kappa=model1.kappa,
			tau0=model1.tau)

		# generate D random documents of length up to N
		D = 100
		N = 600
		docs1 = []
		for _ in range(D):
			wordids = random.permutation(model1.num_words)[:1 + random.randint(N)]
			docs1.append([
				(w, random.randint(10)) for w in wordids])
		docs0 = [zip(*doc) for doc in docs1]

		initial_gamma = random.gamma(100., 1./100., [model1.num_topics, D])

		start = time()
		gamma0, _ = model0.do_e_step(docs0, max_steps=100, gamma=initial_gamma.T)
		time0 = time() - start

		start = time()
		gamma1, _ = model1.do_e_step(docs1, max_iter=100, gamma=initial_gamma)
		time1 = time() - start

		print time1 / time0

		# make sure that C++ implementation is actually faster than Python version
		self.assertLess(time1, time0,
			msg='Inference step took longer ({0:.2f} s) than reference implementation ({1:.2f})'.format(time1, time0))



if __name__ == '__main__':
	unittest.main()
