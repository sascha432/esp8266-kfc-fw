/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>
#include <memory>
#include <StringDepulicator.h>

#undef DEFAULT
#undef min
#undef max
#undef _min
#undef _max

namespace FormUI {

	enum class Type : uint8_t {
		NONE,
		SELECT,
		TEXT,
		NUMBER,
		INTEGER,
		FLOAT,
		RANGE,
		RANGE_SLIDER,
		PASSWORD,
		NEW_PASSWORD,
		// VISIBLE_PASSWORD,
		GROUP_START,
		GROUP_END,
		GROUP_START_DIV,
		GROUP_END_DIV,
		GROUP_START_HR,
		GROUP_END_HR,
		GROUP_START_CARD,
		GROUP_END_CARD,
		HIDDEN,
	};

    enum class StyleType : uint8_t {
        DEFAULT = 0,
        ACCORDION,
        MAX
    };

	class Config;
	using ConfigPtr = std::unique_ptr<Config>;

	class Config {
	public:
		Config(const Config &) = delete;

		Config() : _style(StyleType::DEFAULT), _saveButtonLabel(F("Save Changes...")), _strings() {}

		void setStyle(StyleType style) {
			_style = style;
		}
		void setTitle(const String &title) {
			_title = title;
		}
		void setSaveButtonLabel(const String &label) {
			_saveButtonLabel = label;
		}
		void setContainerId(const String &id) {
			_containerId = id;
		}

		StyleType getStyle() const {
			return _style;
		}
		const String &getContainerId() const {
			return _containerId;
		}
		bool hasTitle() const {
			return _title.length() != 0;
		}
		const String &getTitle() const {
			return _title;
		}
		bool hasButtonLabel() const {
			return _saveButtonLabel.length() != 0;
		}
		const String &getButtonLabel() const {
			return _saveButtonLabel;
		}

		StringDeduplicator &strings() {
			return _strings;
		}

		const char *encodeHtmlEntities(const char *str, bool attribute);
		const char *encodeHtmlEntities(const String &str, bool attribute) {
			return encodeHtmlEntities(str.c_str(), attribute);
		}

	private:
		StyleType _style;
		String _containerId;
		String _title;
		String _saveButtonLabel;
		StringDeduplicator _strings;
	};

}
