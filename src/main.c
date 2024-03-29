#include <assert.h>
#include <node_api.h>
#include "node-fprintd.h"

static napi_value SupportsBiometricsWrapper(napi_env env, napi_callback_info info) {
    napi_status status;
    napi_value result;
    status = napi_get_boolean(env, supports_biometrics(), &result);
    assert(status == napi_ok);
    return result;
}

static napi_value AuthenticateBiometricWrapper(napi_env env, napi_callback_info info) {
    napi_status status;
    napi_value result;
    status = napi_get_boolean(env, authenticate_biometric(), &result);
    assert(status == napi_ok);
    return result;
}

#define DECLARE_NAPI_METHOD(name, func)         \
    { name, 0, func, 0, 0, 0, napi_default, 0 }

static napi_value Init(napi_env env, napi_value exports) {
    napi_status status;
    napi_property_descriptor bindings[] = {
        DECLARE_NAPI_METHOD("supportsBiometrics", SupportsBiometricsWrapper),
        DECLARE_NAPI_METHOD("authenticateBiometric", AuthenticateBiometricWrapper)
    };
    status = napi_define_properties(env, exports, sizeof(bindings) / sizeof(bindings[0]), bindings);
    assert(status == napi_ok);
    return exports;
}

NAPI_MODULE(node_fprintd, Init)
