#include "distributioninterface.h"
#include "Eigen/Core"

#include "mdlda/utils"
using MDLDA::Exception;

PyObject* Distribution_new(PyTypeObject* type, PyObject* args, PyObject* kwds) {
	PyObject* self = type->tp_alloc(type, 0);

	if(self)
		reinterpret_cast<DistributionObject*>(self)->dist = 0;

	return self;
}



const char* Distribution_doc =
	"Abstract base class for distributions.\n";

int Distribution_init(DistributionObject*, PyObject*, PyObject*) {
	PyErr_SetString(PyExc_NotImplementedError, "This is an abstract class.");
	return -1;
}



void Distribution_dealloc(DistributionObject* self) {
	// delete actual instance
	if(self->dist)
		delete self->dist;

	// delete DistributionObject
	self->ob_type->tp_free(reinterpret_cast<PyObject*>(self));
}
