#ifndef CPROGRAMSETTINGS_H
#define CPROGRAMSETTINGS_H

#include <map>

#include "CString.h"
//#include "ICommon.h"

class CProgramSettingsCategory
{
	public:
		CProgramSettingsCategory(const CString& category);
		~CProgramSettingsCategory();

		const CString& get_category_name() const
		{
			return category_name;
		}

		std::map<CString, CString>& get_settings()
		{
			return settings;
		}

		bool exist(const CString& setting) const;

		CString get(const CString& setting) const;
		int get_int(const CString& setting) const;
		float get_float(const CString& setting) const;
		bool get_bool(const CString& setting) const;

		void set(const CString& setting, const char* value);
		void set(const CString& setting, const CString& value);
		void set(const CString& setting, int value);
		void set(const CString& setting, float value);
		void set(const CString& setting, bool value);

	private:
		CString category_name;
		std::map<CString, CString> settings;
};

class CProgramSettings
{
	public:
		CProgramSettings();
		CProgramSettings(const CString& f);
		~CProgramSettings();

		bool load_from_file(const CString& f);
		bool write_to_file(const CString& filename);
		CProgramSettingsCategory* get(const CString& category);
		CProgramSettingsCategory* add(const CString& category);

		CString get(const CString& category, const CString& setting);
		int get_int(const CString& category, const CString& setting);
		float get_float(const CString& category, const CString& setting);
		bool get_bool(const CString& category, const CString& setting);

	private:
		std::map<CString, CProgramSettingsCategory*> categories;
};

#endif
