#ifndef LDAINTERFACE_H
#define LDAINTERFACE_H

#define PY_ARRAY_UNIQUE_SYMBOL MDLDA_ARRAY_API
#define NO_IMPORT_ARRAY
#define NPY_NO_DEPRECATED_API NPY_1_7_API_VERSION

#include <Python.h>
#include <arrayobject.h>

#include "mdlda/models"
using MDLDA::OnlineLDA;

extern const char* OnlineLDA_doc;
extern const char* OnlineLDA_update_parameters_doc;
extern const char* OnlineLDA_update_variables_doc;

struct OnlineLDAObject {
	PyObject_HEAD
	OnlineLDA* lda;
};

int OnlineLDA_init(OnlineLDAObject*, PyObject*, PyObject*);

PyObject* OnlineLDA_num_topics(OnlineLDAObject*, void*);
PyObject* OnlineLDA_num_words(OnlineLDAObject*, void*);
PyObject* OnlineLDA_num_documents(OnlineLDAObject*, void*);
int OnlineLDA_set_num_documents(OnlineLDAObject*, PyObject*, void*);

PyObject* OnlineLDA_lambda(OnlineLDAObject*, void*);
int OnlineLDA_set_lambda(OnlineLDAObject*, PyObject*, void*);

PyObject* OnlineLDA_alpha(OnlineLDAObject*, void*);
int OnlineLDA_set_alpha(OnlineLDAObject*, PyObject*, void*);

PyObject* OnlineLDA_eta(OnlineLDAObject*, void*);
int OnlineLDA_set_eta(OnlineLDAObject*, PyObject*, void*);

PyObject* OnlineLDA_tau(OnlineLDAObject*, void*);
int OnlineLDA_set_tau(OnlineLDAObject*, PyObject*, void*);

PyObject* OnlineLDA_kappa(OnlineLDAObject*, void*);
int OnlineLDA_set_kappa(OnlineLDAObject*, PyObject*, void*);

PyObject* OnlineLDA_update_parameters(OnlineLDAObject*, PyObject*, PyObject*);
PyObject* OnlineLDA_update_variables(OnlineLDAObject*, PyObject*, PyObject*);

PyObject* OnlineLDA_reduce(OnlineLDAObject*, PyObject*);
PyObject* OnlineLDA_setstate(OnlineLDAObject*, PyObject*);

#endif
