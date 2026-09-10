// SPDX-License-Identifier: GPL-3.0-or-later
//
// src/domains/input-engine/backend/libpinyin_wrapper.hpp
//
// 依据：docs/plan/01-m0-tsf-skeleton-plan.md §3.5（M0 建空文件 + TODO，M1 填）
//       docs/plan/02-m1-libpinyin-quanpin-plan.md
//
// M0 占位：不接入 libpinyin。M1 在此封装 pinyin_init / pinyin_guess_sentence /
// pinyin_free_instance 等，向 logic 层暴露不含 glib 类型的接口（DbBackend 适配点见
// system-overview §6）。

#ifndef MYABC_ENGINE_LIBPINYIN_WRAPPER_HPP
#define MYABC_ENGINE_LIBPINYIN_WRAPPER_HPP

namespace myabc::engine {

// TODO(M1): class PinyinEngine { bool Init(model_dir, user_dir); ... };

}  // namespace myabc::engine

#endif  // MYABC_ENGINE_LIBPINYIN_WRAPPER_HPP
