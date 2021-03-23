/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>
#include <memory>
#include <StringDepulicator.h>
#include <PrintHtmlEntities.h>
#include "Types.h"

#include "Utility/Debug.h"

namespace FormUI {

    namespace WebUI {

        class Config {
        public:
            using Mode = PrintHtmlEntities::Mode;

            Config(const Config &) = delete;

            Config(StringDeduplicator &strings) :
                _style(StyleType::DEFAULT),
                _containerId(nullptr),
                _title(nullptr),
                _saveButtonLabel(kDefaultButtonLabel),
                _strings(strings)
            {
            }

            void setStyle(StyleType style) {
                _style = style;
            }

            void setTitle(const __FlashStringHelper *title) {
                __LDBG_assert_printf(_title == nullptr, "title=%s new_title=%s", _title, title);
                _title = encodeHtmlEntities(title);
            }
            void setTitle(const String &title) {
                __LDBG_assert_printf(_title == nullptr, "title=%s new_title=%s", _title, title.c_str());
                _title = encodeHtmlEntities(title);
            }

            void setSaveButtonLabel(const __FlashStringHelper *label) {
                __LDBG_assert_printf(_saveButtonLabel <= kDefaultButtonLabel, "label=%s new_label=%s", _saveButtonLabel, label);
                _saveButtonLabel = encodeHtmlEntities(label);
            }
            void setSaveButtonLabel(const String &label) {
                __LDBG_assert_printf(_saveButtonLabel <= kDefaultButtonLabel, "label=%s new_label=%s", _saveButtonLabel, label.c_str());
                _saveButtonLabel = encodeHtmlEntities(label);
            }

            void setContainerId(const __FlashStringHelper *id) {
                __LDBG_assert_printf(_containerId == nullptr, "container_id=%s new_container_id=%s", _saveButtonLabel, id);
                _containerId = attachString(id);
            }
            void setContainerId(const String &id) {
                __LDBG_assert_printf(_containerId == nullptr, "container_id=%s new_container_id=%s", _saveButtonLabel, id.c_str());
                _containerId = attachString(id);
            }

            StyleType getStyle() const {
                return _style;
            }
            const char *getContainerId() const {
                return _containerId ? _containerId : emptyString.c_str();
            }

            bool hasTitle() const {
                return _title != nullptr;
            }
            const char *getTitle() const {
                return _title ? _title : emptyString.c_str();
            }

            bool hasButtonLabel() const {
                return _saveButtonLabel != nullptr;
            }
            const char *getButtonLabel() const {
                if (_saveButtonLabel == kDefaultButtonLabel) {
                    return _strings.attachString(PSTR("Save Changes..."));
                }
                else if (_saveButtonLabel) {
                    return _saveButtonLabel;
                }
                return emptyString.c_str();
            }

            inline StringDeduplicator &strings() {
                return _strings;
            }

            const char *encodeHtmlEntities(const char *str, Mode mode);

            inline const char *attachString(const char *str) {
                return _strings.attachString(str);
            }
            inline const char *attachString(const __FlashStringHelper *fpstr) {
                return _strings.attachString(fpstr);
            }
            inline const char *attachString(const String &str) {
                return _strings.attachString(str);
            }

            inline const char *encodeHtmlEntities(const char *str) {
                return encodeHtmlEntities(str, Mode::HTML);
            }
            inline const char *encodeHtmlEntities(const __FlashStringHelper *fpstr) {
                return encodeHtmlEntities((PGM_P)fpstr);
            }
            inline const char *encodeHtmlEntities(const String &str) {
                return encodeHtmlEntities(str.c_str());
            }

            inline const char *encodeHtmlAttribute(const char *str) {
                return encodeHtmlEntities(str, Mode::ATTRIBUTE);
            }
            inline const char *encodeHtmlAttribute(const __FlashStringHelper *fpstr) {
                return encodeHtmlAttribute((PGM_P)fpstr);
            }
            inline const char *encodeHtmlAttribute(const String &str) {
                return encodeHtmlAttribute(str.c_str());
            }

        private:
            static constexpr const char *kDefaultButtonLabel = (const char *)1;

            StyleType _style;
            const char *_containerId;
            const char *_title;
            const char *_saveButtonLabel;
            StringDeduplicator &_strings;
        };

    }

}

#include <debug_helper_disable.h>
