//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_runtime/binding/go/internal/common.h"
#include "include_internal/ten_runtime/binding/go/msg/msg.h"
#include "include_internal/ten_runtime/binding/go/ten_env/ten_env.h"
#include "include_internal/ten_runtime/binding/go/ten_env/ten_env_internal.h"
#include "include_internal/ten_runtime/ten_env/ten_env.h"
#include "ten_runtime/binding/go/interface/ten_runtime/common.h"
#include "ten_runtime/binding/go/interface/ten_runtime/msg.h"
#include "ten_runtime/binding/go/interface/ten_runtime/ten_env.h"
#include "ten_runtime/common/error_code.h"
#include "ten_runtime/ten_env_proxy/ten_env_proxy.h"
#include "ten_utils/lib/alloc.h"
#include "ten_utils/lib/error.h"
#include "ten_utils/lib/smart_ptr.h"
#include "ten_utils/macro/check.h"
#include "ten_utils/macro/mark.h"

typedef struct ten_env_notify_send_data_ctx_t {
  ten_shared_ptr_t *c_data;
  ten_go_handle_t callback_handle;
} ten_env_notify_send_data_ctx_t;

static ten_env_notify_send_data_ctx_t *ten_env_notify_send_data_ctx_create(
    ten_shared_ptr_t *c_data, ten_go_handle_t callback_handle) {
  TEN_ASSERT(c_data, "Invalid argument.");

  ten_env_notify_send_data_ctx_t *ctx =
      TEN_MALLOC(sizeof(ten_env_notify_send_data_ctx_t));
  TEN_ASSERT(ctx, "Failed to allocate memory.");

  ctx->c_data = c_data;
  ctx->callback_handle = callback_handle;

  return ctx;
}

static void ten_env_notify_send_data_ctx_destroy(
    ten_env_notify_send_data_ctx_t *ctx) {
  TEN_ASSERT(ctx, "Invalid argument.");

  if (ctx->c_data) {
    ten_shared_ptr_destroy(ctx->c_data);
    ctx->c_data = NULL;
  }

  ctx->callback_handle = 0;

  TEN_FREE(ctx);
}

static void proxy_handle_data_error(ten_env_t *ten_env,
                                    TEN_UNUSED ten_shared_ptr_t *c_cmd_result,
                                    void *callback_info_, ten_error_t *err) {
  ten_go_callback_ctx_t *callback_info = callback_info_;
  TEN_ASSERT(callback_info, "Should not happen.");

  ten_go_error_t cgo_error;
  TEN_GO_ERROR_INIT(cgo_error);

  if (err) {
    ten_go_error_set_from_error(&cgo_error, err);
  }

  TEN_ASSERT(callback_info->callback_id != TEN_GO_NO_RESPONSE_HANDLER,
             "Should not happen.");

  ten_go_ten_env_t *ten_env_bridge = ten_go_ten_env_wrap(ten_env);

  tenGoOnError(ten_env_bridge->bridge.go_instance, callback_info->callback_id,
               cgo_error);

  ten_go_callback_ctx_destroy(callback_info);
}

static void ten_env_proxy_notify_send_data(ten_env_t *ten_env,
                                           void *user_data) {
  TEN_ASSERT(user_data, "Invalid argument.");
  TEN_ASSERT(ten_env, "Should not happen.");
  TEN_ASSERT(ten_env_check_integrity(ten_env, true), "Should not happen.");

  ten_env_notify_send_data_ctx_t *notify_info = user_data;
  TEN_ASSERT(notify_info, "Should not happen.");

  ten_go_error_t cgo_error;
  TEN_GO_ERROR_INIT(cgo_error);

  ten_error_t err;
  TEN_ERROR_INIT(err);

  bool res = false;

  if (notify_info->callback_handle == TEN_GO_NO_RESPONSE_HANDLER) {
    res = ten_env_send_data(ten_env, notify_info->c_data, NULL, NULL, &err);
  } else {
    ten_go_callback_ctx_t *ctx =
        ten_go_callback_ctx_create(notify_info->callback_handle);
    res = ten_env_send_data(ten_env, notify_info->c_data,
                            proxy_handle_data_error, ctx, &err);

    if (!res) {
      // Prepare error information to pass to Go.
      ten_go_error_set_from_error(&cgo_error, &err);

      ten_go_ten_env_t *ten_env_bridge = ten_go_ten_env_wrap(ten_env);

      tenGoOnError(ten_env_bridge->bridge.go_instance,
                   notify_info->callback_handle, cgo_error);

      ten_go_callback_ctx_destroy(ctx);
    }
  }

  ten_error_deinit(&err);

  // The notify_info is no longer needed.
  ten_env_notify_send_data_ctx_destroy(notify_info);
}

ten_go_error_t ten_go_ten_env_send_data(uintptr_t bridge_addr,
                                        uintptr_t data_bridge_addr,
                                        ten_go_handle_t handler_id) {
  ten_go_ten_env_t *self = ten_go_ten_env_reinterpret(bridge_addr);
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_go_ten_env_check_integrity(self), "Should not happen.");

  ten_go_msg_t *data = ten_go_msg_reinterpret(data_bridge_addr);
  TEN_ASSERT(data, "Should not happen.");
  TEN_ASSERT(ten_go_msg_check_integrity(data), "Should not happen.");
  TEN_ASSERT(ten_go_msg_c_msg(data), "Should not happen.");

  ten_go_error_t cgo_error;
  TEN_GO_ERROR_INIT(cgo_error);

  TEN_GO_TEN_ENV_IS_ALIVE_REGION_BEGIN(self, {
    ten_go_error_set_error_code(&cgo_error, TEN_ERROR_CODE_TEN_IS_CLOSED);
  });

  ten_error_t err;
  TEN_ERROR_INIT(err);

  ten_env_notify_send_data_ctx_t *notify_info =
      ten_env_notify_send_data_ctx_create(
          ten_go_msg_move_c_msg(data),
          handler_id <= 0 ? TEN_GO_NO_RESPONSE_HANDLER : handler_id);

  if (!ten_env_proxy_notify(self->c_ten_env_proxy,
                            ten_env_proxy_notify_send_data, notify_info, false,
                            &err)) {
    // Failed to invoke ten_env_proxy_notify.
    ten_env_notify_send_data_ctx_destroy(notify_info);
    ten_go_error_set_from_error(&cgo_error, &err);
  }

  ten_error_deinit(&err);

  TEN_GO_TEN_ENV_IS_ALIVE_REGION_END(self);

ten_is_close:
  return cgo_error;
}
