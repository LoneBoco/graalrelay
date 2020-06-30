#include <map>
#include <sstream>

#include "CProgramSettings.h"

/////////////////////////////

CProgramSettingsCategory::CProgramSettingsCategory(const CString& category)
: category_name(category)
{
}

CProgramSettingsCategory::~CProgramSettingsCategory()
{
}

bool CProgramSettingsCategory::exist(const CString& setting) const
{
	std::map<CString, CString>::const_iterator i = settings.find(setting);
	if (i == settings.end()) return false;
	return true;
}

CString CProgramSettingsCategory::get(const CString& setting) const
{
	std::map<CString, CString>::const_iterator i = settings.find(setting);
	if (i == settings.end()) return CString();
	return i->second;
}

int CProgramSettingsCategory::get_int(const CString& setting) const
{
	std::map<CString, CString>::const_iterator i = settings.find(setting);
	if (i == settings.end()) return 0;

	std::istringstream str((char*)i->second.text());
	int v;
	str >> v;
	return v;
}

float CProgramSettingsCategory::get_float(const CString& setting) const
{
	std::map<CString, CString>::const_iterator i = settings.find(setting);
	if (i == settings.end()) return 0.0f;

	std::istringstream str((char*)i->second.text());
	float v;
	str >> v;
	return v;
}

bool CProgramSettingsCategory::get_bool(const CString& setting) const
{
	std::map<CString, CString>::const_iterator i = settings.find(setting);
	if (i == settings.end()) return false;

	if (i->second == "false") return false;
	else return true;
}

void CProgramSettingsCategory::set(const CString& setting, const char* value)
{
	settings[setting] = value;
}

void CProgramSettingsCategory::set(const CString& setting, const CString& value)
{
	settings[setting] = value;
}

void CProgramSettingsCategory::set(const CString& setting, int value)
{
	std::stringstream str;
	str << value;
	settings[setting] = CString(str.str().c_str());
}

void CProgramSettingsCategory::set(const CString& setting, float value)
{
	std::stringstream str;
	str << value;
	settings[setting] = CString(str.str().c_str());
}

void CProgramSettingsCategory::set(const CString& setting, bool value)
{
	if (value)
		settings[setting] = "true";
	else settings[setting] = "false";
}

/////////////////////////////

CProgramSettings::CProgramSettings()
{
}

CProgramSettings::CProgramSettings(const CString& f)
{
	load_from_file(f);
}

CProgramSettings::~CProgramSettings()
{
	for (std::map<CString, CProgramSettingsCategory*>::iterator i = categories.begin(); i != categories.end(); ++i)
		delete i->second;
	categories.clear();
}

bool CProgramSettings::load_from_file(const CString& f)
{
	CProgramSettingsCategory* category = 0;
	CString filedata;
	filedata.load(f);
	
	filedata.removeAllI("\r");
	std::vector<CString> lines = filedata.tokenize("\n");

	for (std::vector<CString>::iterator i = lines.begin(); i != lines.end(); ++i)
	{
		// Read the line and trim it.
		CString line = *i;
		line.trimI();

		// Remove any comments.
		int comment = line.find("//");
		if (comment != -1)
			line.removeI(comment);

		// Check for an empty line now.
		if (line.isEmpty()) continue;

		// Check if this is a category or an item.
		if (line[0] == '[' && line[line.length() - 1] == ']')
		{
			// Save the old category.
			if (category != 0)
				categories[category->get_category_name()] = category;

			// Create a new category.
			category = new CProgramSettingsCategory(line.subString(1, line.length() - 2));
		}
		else
		{
			if (category == 0) continue;

			// Parse our string.
			int pos_e = line.find("=");
			if (pos_e == -1) continue;

			CString setting = line.subString(0, pos_e).trim();
			CString value = line.subString(pos_e + 1).trim();

			// Save it to our current category.
			category->set(setting, value);
		}
	}

	// Save the category.
	if (category != 0)
		categories[category->get_category_name()] = category;

	return true;
}

bool CProgramSettings::write_to_file(const CString& filename)
{
	// Assemble the file.
	CString filedata;
	for (std::map<CString, CProgramSettingsCategory*>::iterator i = categories.begin(); i != categories.end(); ++i)
	{
		CProgramSettingsCategory* category = i->second;
		if (category == 0) continue;

		filedata << "[" << category->get_category_name() << "]\r\n";
		
		std::map<CString, CString> settings_map = category->get_settings();
		for (std::map<CString, CString>::iterator j = settings_map.begin(); j != settings_map.end(); ++j)
		{
			filedata << j->first << " = " << j->second << "\r\n";
		}

		filedata += "\r\n";
	}
	if (filedata.isEmpty()) return true;

	// Save the file.
	filedata.save(filename);
	return true;
}

CProgramSettingsCategory* CProgramSettings::get(const CString& category)
{
	std::map<CString, CProgramSettingsCategory*>::iterator i = categories.find(category);
	if (i == categories.end()) return 0;
	return i->second;
}

CProgramSettingsCategory* CProgramSettings::add(const CString& category)
{
	std::map<CString, CProgramSettingsCategory*>::iterator i = categories.find(category);
	if (i != categories.end()) return i->second;

	CProgramSettingsCategory* c = new CProgramSettingsCategory(category);
	categories[category] = c;
	return c;
}

CString CProgramSettings::get(const CString& category, const CString& setting)
{
	CProgramSettingsCategory* cat = get(category);
	if (!cat) return CString();
	return cat->get(setting);
}

int CProgramSettings::get_int(const CString& category, const CString& setting)
{
	CProgramSettingsCategory* cat = get(category);
	if (!cat) return 0;
	return cat->get_int(setting);
}

float CProgramSettings::get_float(const CString& category, const CString& setting)
{
	CProgramSettingsCategory* cat = get(category);
	if (!cat) return 0.0f;
	return cat->get_float(setting);
}

bool CProgramSettings::get_bool(const CString& category, const CString& setting)
{
	CProgramSettingsCategory* cat = get(category);
	if (!cat) return false;
	return cat->get_bool(setting);
}
