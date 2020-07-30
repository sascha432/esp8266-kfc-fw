/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

namespace FormUI {

	class Config;
	using ConfigPtr = std::unique_ptr<Config>;

	class Config {
	public:
		Config() : _style(StyleType::DEFAULT), _saveButtonLabel(F("Save Changes...")) {}

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
			return _title;
		}

	private:
		StyleType _style;
		String _containerId;
		String _title;
		String _saveButtonLabel;
	};

}
