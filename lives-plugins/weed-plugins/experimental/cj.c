#include "cj.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// temporary: help me track mem usage
#ifdef CJ_MEM
#define CJ_ALLOC(x) printf("l+0x%x\n", (x))
#define CJ_FREE(x) printf("l-0x%x\n", (x))
#define CJ_GLO_ALLOC(x) printf("g+0x%x\n", (x))
#define CJ_GLO_FREE(x) printf("g-0x%x\n", (x))
#else
#define CJ_ALLOC(x)
#define CJ_FREE(x)
#define CJ_GLO_ALLOC(x)
#define CJ_GLO_FREE(x)
#endif

jboolean checkException(cjJVM_t *pJVM);
int callProxyMethod(cjObject_t *pProxy, jobject data, jobject *pOutData,
                    int methodIndex);
int acquireGlobalReference(cjJVM_t *pJVM, jobject loObj, jobject *pGloObj);
int jstring2cstring(cjJVM_t *pJVM, jstring js, char *cs);

// Proxy class spec

#define CJ_PROXY_CONSTRUCTOR_METHOD 0
#define CJ_PROXY_START_METHOD 1
#define CJ_PROXY_END_METHOD 2
#define CJ_PROXY_ADD_INSTANCE_METHOD 3
#define CJ_PROXY_BUILD_MODEL_METHOD 4
#define CJ_PROXY_SAVE_MODEL_METHOD 5
#define CJ_PROXY_LOAD_MODEL_METHOD 6
#define CJ_PROXY_RUN_MODEL_METHOD 7
#define CJ_PROXY_RESET_MODEL_METHOD 8

static cjMethod_t proxyMethods[] = {
  {"<init>", "()V", NULL},
  {"start", "(Ljava/lang/Object;)Ljava/lang/Object;", NULL},
  {"end", "(Ljava/lang/Object;)Ljava/lang/Object;", NULL},
  {"addInstance", "(Ljava/lang/Object;)Ljava/lang/Object;", NULL},
  {"buildModel", "(Ljava/lang/Object;)Ljava/lang/Object;", NULL},
  {"saveModel", "(Ljava/lang/Object;)Ljava/lang/Object;", NULL},
  {"loadModel", "(Ljava/lang/Object;)Ljava/lang/Object;", NULL},
  {"runModel", "(Ljava/lang/Object;)Ljava/lang/Object;", NULL},
  {"resetModel", "(Ljava/lang/Object;)Ljava/lang/Object;", NULL}
};

// java.lang.Integer class spec

#define CJ_INT_CONSTRUCTOR_METHOD 0
#define CJ_INT_INTVALUE_METHOD 1

static cjMethod_t integerMethods[] = {
  {"<init>", "(I)V", NULL},
  {"intValue", "()I", NULL}
};

// java.lang.Boolean class spec

#define CJ_BOOLEAN_CONSTRUCTOR_METHOD 0
#define CJ_BOOLEAN_BOOLEANVALUE_METHOD 1

static cjMethod_t booleanMethods[] = {
  {"<init>", "(Z)V", NULL},
  {"booleanValue", "()Z", NULL}
};

/*
 * Connect to JVM using arguments in pJVM.  If failure, clean up.
 * Returns CJ_ERR_SUCCESS if sucessful.
 */
int cjJVMConnect(cjJVM_t *pJVM) {
  long jret = 0;
  int rc = CJ_ERR_SUCCESS;

  JavaVMInitArgs vmArgs;

  JavaVMOption *options = malloc(pJVM->argc * sizeof(JavaVMOption));
  int i = 0;
  for (i = 0; i < pJVM->argc; i++) {
    options[i].optionString = (pJVM->argv)[i];
    options[i].extraInfo = NULL;
  }

  memset(&vmArgs, 0, sizeof(vmArgs));
  vmArgs.version = JNI_VERSION_1_2;
  vmArgs.nOptions = pJVM->argc;
  vmArgs.options = options;
  vmArgs.ignoreUnrecognized = JNI_FALSE;

  pJVM->ok = JNI_FALSE;

  // create VM
  if (rc == CJ_ERR_SUCCESS) {
    JavaVM *jvm;
    JNIEnv *jni;

    jret = JNI_CreateJavaVM(&jvm, (void **)&jni, &vmArgs);
    free(options);
    if (jret == JNI_ERR) {
      rc = CJ_ERR_JVM_CONNECT;
    } else {
      pJVM->jvm = jvm;
      pJVM->jni = jni;
      pJVM->ok = JNI_TRUE;
    }
  }

  // if we failed, cleanup
  if (rc != CJ_ERR_SUCCESS) {
    cjJVMDisconnect(pJVM);
  }

  return rc;
}

/*
 * Destroys JVM.  Returns CJ_ERR_SUCCESS if sucessful
 */
int cjJVMDisconnect(cjJVM_t *pJVM) {
  // Disconnect from JVM
  int rc = CJ_ERR_SUCCESS;
  JavaVM *jvm = pJVM->jvm;
  pJVM->ok = JNI_FALSE;

  if (jvm != NULL) {
    (*jvm)->DetachCurrentThread(jvm);
    (*jvm)->DestroyJavaVM(jvm);
  }
  return rc;
}

/*
 * Load given class and get its methods
 */
int cjClassCreate(cjClass_t *pClass) {
  int rc = CJ_ERR_SUCCESS;
  cjJVM_t *jvm = pClass->jvm;
  JNIEnv *env = jvm->jni;
  jboolean isException = JNI_FALSE;
  int i = 0;

  pClass->ok = JNI_FALSE;

  // Get class
  if (rc == CJ_ERR_SUCCESS) {
    pClass->clazz = (*env)->FindClass(env, pClass->className);
    isException = checkException(jvm);
    if (isException || pClass->clazz == NULL) {
      rc = CJ_ERR_JNI;
    }
  }

  // TODO -- do I need global ref to class?

  // Get methods
  //for (i = 0; i < pClass->numMethods; i++)
  for (i = 0; i < 9; i++) {
    if (rc == CJ_ERR_SUCCESS) {
      cjMethod_t *pMethod = &((pClass->methods)[i]);

      pMethod->method = (*env)->GetMethodID(env, pClass->clazz,
                                            pMethod->methodName, pMethod->methodSig);

      printf("got method %s\n",pMethod->methodName);

      isException = checkException(jvm);
      if (isException || pMethod->method == NULL) {
        rc = CJ_ERR_JNI;
      }
    }
  }

  // if success
  if (rc == CJ_ERR_SUCCESS) {
    pClass->ok = JNI_TRUE;
  }

  return rc;
}

/*
 * Destroy the given class.
 * Nothing to do -- don't have to cleanup methods or class?
 */
int cjClassDestroy(cjClass_t *pClass) {
  pClass->ok = JNI_FALSE;
  return CJ_ERR_SUCCESS;

  // TODO -- Do i need to free global ref to class
}

/**
 * Frees an object for which there is a global reference
 */
int cjFreeObject(cjJVM_t *pJVM, jobject gloObj) {
  int rc = CJ_ERR_SUCCESS;
  JNIEnv *env = pJVM->jni;
  jboolean isException = JNI_FALSE;

  // delete global ref if it exists
  if (rc == CJ_ERR_SUCCESS) {
    CJ_GLO_FREE(gloObj);
    (*env)->DeleteGlobalRef(env, gloObj);
    isException = checkException(pJVM);
    if (isException) {
      rc = CJ_ERR_JNI;
    }
  }
  return rc;
}

/**
 * Creates a proxy class with the given class name.
 * The methods are given by the proxy spec above.
 */
int cjProxyClassCreate(cjClass_t *pClass, char *className,
                       cjJVM_t *pJVM) {
  pClass->className = className;
  pClass->jvm = pJVM;
  pClass->numMethods = 2;
  pClass->methods = proxyMethods;
  return cjClassCreate(pClass);
}

/*
 * Instantiate a proxy
 */
int cjProxyCreate(cjObject_t *pProxy) {
  int rc = CJ_ERR_SUCCESS;
  cjClass_t *pClass = pProxy->clazz;
  cjJVM_t *pJVM = pClass->jvm;
  JNIEnv *env = pJVM->jni;
  jboolean isException = JNI_FALSE;
  jobject loObj = NULL;

  pProxy->ok = JNI_FALSE;

  // Get local reference, from this get global reference
  if (rc == CJ_ERR_SUCCESS) {
    loObj = (*env)->NewObject(env, pClass->clazz,
                              ((pClass->methods)[CJ_PROXY_CONSTRUCTOR_METHOD]).method);
    isException = checkException(pJVM);
    if (isException) {
      rc = CJ_ERR_JNI;
    }

    rc = acquireGlobalReference(pJVM, loObj, &(pProxy->object));
    isException = checkException(pJVM);
    if (isException) {
      rc = CJ_ERR_JNI;
    }
  }

  // if we failed, cleanup
  if (rc == CJ_ERR_SUCCESS) {
    pProxy->ok = JNI_TRUE;
  }
  return rc;
}

int cjProxyStart(cjObject_t *pProxy, jobject data, jobject *pOutData) {
  return callProxyMethod(pProxy, data, pOutData, CJ_PROXY_START_METHOD);
}

int cjProxyEnd(cjObject_t *pProxy, jobject data, jobject *pOutData) {
  return callProxyMethod(pProxy, data, pOutData, CJ_PROXY_END_METHOD);
}

int cjProxyAddInstance(cjObject_t *pProxy, jobject data, jobject *pOutData) {
  return callProxyMethod(pProxy, data, pOutData, CJ_PROXY_ADD_INSTANCE_METHOD);
}

int cjProxyBuildModel(cjObject_t *pProxy, jobject data, jobject *pOutData) {
  return callProxyMethod(pProxy, data, pOutData, CJ_PROXY_BUILD_MODEL_METHOD);
}

int cjProxySaveModel(cjObject_t *pProxy, jobject data, jobject *pOutData) {
  return callProxyMethod(pProxy, data, pOutData, CJ_PROXY_SAVE_MODEL_METHOD);
}

int cjProxyLoadModel(cjObject_t *pProxy, jobject data, jobject *pOutData) {
  return callProxyMethod(pProxy, data, pOutData, CJ_PROXY_LOAD_MODEL_METHOD);
}

int cjProxyRunModel(cjObject_t *pProxy, jobject data, jobject *pOutData) {
  return callProxyMethod(pProxy, data, pOutData, CJ_PROXY_RUN_MODEL_METHOD);
}

int cjProxyResetModel(cjObject_t *pProxy, jobject data, jobject *pOutData) {
  return callProxyMethod(pProxy, data, pOutData, CJ_PROXY_RESET_MODEL_METHOD);
}

int callProxyMethod(cjObject_t *pProxy, jobject data, jobject *pOutData,
                    int methodIndex) {
  int rc = CJ_ERR_SUCCESS;
  cjClass_t *pClass = pProxy->clazz;
  cjJVM_t *jvm = pClass->jvm;
  JNIEnv *env = jvm->jni;
  jboolean isException = JNI_FALSE;
  jobject loRef = NULL;

  // Call method
  if (rc == CJ_ERR_SUCCESS) {
    loRef = (*env)->CallObjectMethod(env, pProxy->object,
                                     (pClass->methods[methodIndex]).method, data);
    isException = checkException(jvm);
    if (isException) {
      rc = CJ_ERR_JNI;
    }

    rc = acquireGlobalReference(jvm, loRef, pOutData);
  }

  return rc;
}



int cjProxyStartString(cjObject_t *pProxy, char *inData, char *outData) {
  int rc = CJ_ERR_SUCCESS;
  cjClass_t *pClass = pProxy->clazz;
  cjJVM_t *jvm = pClass->jvm;
  JNIEnv *env = jvm->jni;
  jboolean isException = JNI_FALSE;
  jstring instring = NULL;
  jstring outobject;

  // Create string
  if (rc == CJ_ERR_SUCCESS) {
    instring = (*env)->NewStringUTF(env, inData);
    if (instring == NULL) {
      rc = CJ_ERR_MEM;
    }
    isException = checkException(jvm);
    if (isException) {
      rc = CJ_ERR_JNI;
    }
  }

  // Call proxy exec on string
  if (rc == CJ_ERR_SUCCESS) {
    rc = cjProxyStart(pProxy, instring, &outobject);
  }

  // copy return object into a buffer
  if (rc == CJ_ERR_SUCCESS) {
    rc = jstring2cstring(jvm, outobject, outData);
  }

  // free the string if it was created
  if (instring != NULL) {
    // nothing to do
  }

  return rc;
}


int cjProxyEndString(cjObject_t *pProxy, char *inData, char *outData) {
  int rc = CJ_ERR_SUCCESS;
  cjClass_t *pClass = pProxy->clazz;
  cjJVM_t *jvm = pClass->jvm;
  JNIEnv *env = jvm->jni;
  jboolean isException = JNI_FALSE;
  jstring instring = NULL;
  jstring outobject;

  // Create string
  if (rc == CJ_ERR_SUCCESS) {
    instring = (*env)->NewStringUTF(env, inData);
    if (instring == NULL) {
      rc = CJ_ERR_MEM;
    }
    isException = checkException(jvm);
    if (isException) {
      rc = CJ_ERR_JNI;
    }
  }

  // Call proxy exec on string
  if (rc == CJ_ERR_SUCCESS) {
    rc = cjProxyEnd(pProxy, instring, &outobject);
  }

  // copy return object into a buffer
  if (rc == CJ_ERR_SUCCESS) {
    rc = jstring2cstring(jvm, outobject, outData);
  }

  // free the string if it was created
  if (instring != NULL) {
    // nothing to do
  }

  return rc;
}


int cjProxyAddInstanceString(cjObject_t *pProxy, char *inData, char *outData) {
  int rc = CJ_ERR_SUCCESS;
  cjClass_t *pClass = pProxy->clazz;
  cjJVM_t *jvm = pClass->jvm;
  JNIEnv *env = jvm->jni;
  jboolean isException = JNI_FALSE;
  jstring instring = NULL;
  jstring outobject;

  // Create string
  if (rc == CJ_ERR_SUCCESS) {
    instring = (*env)->NewStringUTF(env, inData);
    if (instring == NULL) {
      rc = CJ_ERR_MEM;
    }
    isException = checkException(jvm);
    if (isException) {
      rc = CJ_ERR_JNI;
    }
  }

  // Call proxy exec on string
  if (rc == CJ_ERR_SUCCESS) {
    rc = cjProxyAddInstance(pProxy, instring, &outobject);
  }

  // copy return object into a buffer
  if (rc == CJ_ERR_SUCCESS) {
    rc = jstring2cstring(jvm, outobject, outData);
  }

  // free the string if it was created
  if (instring != NULL) {
    // nothing to do
  }

  return rc;
}


int cjProxyBuildModelString(cjObject_t *pProxy, char *inData, char *outData) {
  int rc = CJ_ERR_SUCCESS;
  cjClass_t *pClass = pProxy->clazz;
  cjJVM_t *jvm = pClass->jvm;
  JNIEnv *env = jvm->jni;
  jboolean isException = JNI_FALSE;
  jstring instring = NULL;
  jstring outobject;

  // Create string
  if (rc == CJ_ERR_SUCCESS) {
    instring = (*env)->NewStringUTF(env, inData);
    if (instring == NULL) {
      rc = CJ_ERR_MEM;
    }
    isException = checkException(jvm);
    if (isException) {
      rc = CJ_ERR_JNI;
    }
  }

  // Call proxy exec on string
  if (rc == CJ_ERR_SUCCESS) {
    rc = cjProxyBuildModel(pProxy, instring, &outobject);
  }

  // copy return object into a buffer
  if (rc == CJ_ERR_SUCCESS) {
    rc = jstring2cstring(jvm, outobject, outData);
  }

  // free the string if it was created
  if (instring != NULL) {
    // nothing to do
  }

  return rc;
}


int cjProxySaveModelString(cjObject_t *pProxy, char *inData, char *outData) {
  int rc = CJ_ERR_SUCCESS;
  cjClass_t *pClass = pProxy->clazz;
  cjJVM_t *jvm = pClass->jvm;
  JNIEnv *env = jvm->jni;
  jboolean isException = JNI_FALSE;
  jstring instring = NULL;
  jstring outobject;

  // Create string
  if (rc == CJ_ERR_SUCCESS) {
    instring = (*env)->NewStringUTF(env, inData);
    if (instring == NULL) {
      rc = CJ_ERR_MEM;
    }
    isException = checkException(jvm);
    if (isException) {
      rc = CJ_ERR_JNI;
    }
  }

  // Call proxy exec on string
  if (rc == CJ_ERR_SUCCESS) {
    rc = cjProxySaveModel(pProxy, instring, &outobject);
  }

  // copy return object into a buffer
  if (rc == CJ_ERR_SUCCESS) {
    rc = jstring2cstring(jvm, outobject, outData);
  }

  // free the string if it was created
  if (instring != NULL) {
    // nothing to do
  }

  return rc;
}


int cjProxyLoadModelString(cjObject_t *pProxy, char *inData, char *outData) {
  int rc = CJ_ERR_SUCCESS;
  cjClass_t *pClass = pProxy->clazz;
  cjJVM_t *jvm = pClass->jvm;
  JNIEnv *env = jvm->jni;
  jboolean isException = JNI_FALSE;
  jstring instring = NULL;
  jstring outobject;

  // Create string
  if (rc == CJ_ERR_SUCCESS) {
    instring = (*env)->NewStringUTF(env, inData);
    if (instring == NULL) {
      rc = CJ_ERR_MEM;
    }
    isException = checkException(jvm);
    if (isException) {
      rc = CJ_ERR_JNI;
    }
  }

  // Call proxy exec on string
  if (rc == CJ_ERR_SUCCESS) {
    rc = cjProxyLoadModel(pProxy, instring, &outobject);
  }

  // copy return object into a buffer
  if (rc == CJ_ERR_SUCCESS) {
    rc = jstring2cstring(jvm, outobject, outData);
  }

  // free the string if it was created
  if (instring != NULL) {
    // nothing to do
  }

  return rc;
}


int cjProxyRunModelString(cjObject_t *pProxy, char *inData, char *outData) {
  int rc = CJ_ERR_SUCCESS;
  cjClass_t *pClass = pProxy->clazz;
  cjJVM_t *jvm = pClass->jvm;
  JNIEnv *env = jvm->jni;
  jboolean isException = JNI_FALSE;
  jstring instring = NULL;
  jstring outobject;

  // Create string
  if (rc == CJ_ERR_SUCCESS) {
    instring = (*env)->NewStringUTF(env, inData);
    if (instring == NULL) {
      rc = CJ_ERR_MEM;
    }
    isException = checkException(jvm);
    if (isException) {
      rc = CJ_ERR_JNI;
    }
  }

  // Call proxy exec on string
  if (rc == CJ_ERR_SUCCESS) {
    rc = cjProxyRunModel(pProxy, instring, &outobject);
  }

  // copy return object into a buffer
  if (rc == CJ_ERR_SUCCESS) {
    rc = jstring2cstring(jvm, outobject, outData);
  }

  // free the string if it was created
  if (instring != NULL) {
    // nothing to do
  }

  return rc;
}




int cjProxyResetModelString(cjObject_t *pProxy, char *inData, char *outData) {
  int rc = CJ_ERR_SUCCESS;
  cjClass_t *pClass = pProxy->clazz;
  cjJVM_t *jvm = pClass->jvm;
  JNIEnv *env = jvm->jni;
  jboolean isException = JNI_FALSE;
  jstring instring = NULL;
  jstring outobject;

  // Create string
  if (rc == CJ_ERR_SUCCESS) {
    instring = (*env)->NewStringUTF(env, inData);
    if (instring == NULL) {
      rc = CJ_ERR_MEM;
    }
    isException = checkException(jvm);
    if (isException) {
      rc = CJ_ERR_JNI;
    }
  }

  // Call proxy exec on string
  if (rc == CJ_ERR_SUCCESS) {
    rc = cjProxyResetModel(pProxy, instring, &outobject);
  }

  // copy return object into a buffer
  if (rc == CJ_ERR_SUCCESS) {
    rc = jstring2cstring(jvm, outobject, outData);
  }

  // free the string if it was created
  if (instring != NULL) {
    // nothing to do
  }

  return rc;
}









/**
 * Acquire global reference (pGloObj) based on local reference (loObj).
 * Release the local reference.
 */
int acquireGlobalReference(cjJVM_t *pJVM, jobject loObj, jobject *pGloObj) {
  int rc = CJ_ERR_SUCCESS;
  JNIEnv *env = pJVM->jni;
  jboolean isException = JNI_FALSE;

  if (loObj != NULL) CJ_ALLOC(loObj);
  isException = checkException(pJVM);
  if (isException || loObj == NULL) {
    rc = CJ_ERR_JNI;
  }

  // create global reference
  if (rc == CJ_ERR_SUCCESS) {
    *pGloObj = (*env)->NewGlobalRef(env, loObj);
    if (*pGloObj != NULL) CJ_GLO_ALLOC(*pGloObj);
    isException = checkException(pJVM);
    if (isException || *pGloObj == NULL) {
      rc = CJ_ERR_JNI;
    }
  }

  // delete local reference
  if (loObj != NULL) {
    CJ_FREE(loObj);
    (*env)->DeleteLocalRef(env, loObj);
    isException = checkException(pJVM);
    if (isException) {
      rc = CJ_ERR_JNI;
    }
  }

  return rc;
}

/**
 * Convert jstring to c zero-terminated string
 */
int jstring2cstring(cjJVM_t *pJVM, jstring js, char *cs) {
  int rc = CJ_ERR_SUCCESS;
  JNIEnv *env = pJVM->jni;
  jboolean isException = JNI_FALSE;

  // copy object into a buffer
  if (rc == CJ_ERR_SUCCESS) {
    const char *tempData;

    tempData = (*env)->GetStringUTFChars(env, js, 0);
    if (tempData == NULL) {
      rc = CJ_ERR_MEM;
    }

    isException = checkException(pJVM);
    if (isException) {
      rc = CJ_ERR_JNI;
    }

    if (rc == CJ_ERR_SUCCESS) {
      // copy to caller's buffer and release the UTF
      strcpy(cs, (char *)tempData);
      (*env)->ReleaseStringUTFChars(env, js, tempData);
      isException = checkException(pJVM);
      if (isException) {
        rc = CJ_ERR_JNI;
      }
    }
  }

  return rc;
}

/**
 * Check for exception and clear it.  Return CJ_ERR_SUCCESS if no exception
 */
jboolean checkException(cjJVM_t *pJVM) {
  JNIEnv *env = pJVM->jni;
  jboolean isException = (*env)->ExceptionCheck(env);
  if (isException) {
    (*env)->ExceptionDescribe(env); // capture this somewhere
    (*env)->ExceptionClear(env);
  }
  return isException;
}


