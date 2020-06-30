#ifndef CLOG_H
#define CLOG_H

#include <stdarg.h>
#include <stdio.h>
#include "CString.h"


//! Logger class for logging information to a file.
class CLog
{
	public:
		//! Creates a new CLog that outputs to the specific file.
		//! \param file The file to log to.
		//! \param enabled If the class logs by default to the file or not.
		CLog();
		CLog(const CString& _file, bool _enabled = true);

		//! Cleans up and closes files.
		virtual ~CLog();

		//! Outputs a text string to a file.
		/*!
			Outputs text to a file.  Use like a standard printf() command.
			\param format Format string.
		*/
		void out(const CString format, ...);

		//! Clears the output file.
		void clear();

		//! Gets the enabled state of the class.
		//! \return True if logging is enabled.
		bool getEnabled() const;

		//! Sets the enabled state of the logger class.
		/*!
			Sets the enabled state of the logger class.
			If /a enabled is set to false, it will no longer log to a file.
			\param enabled If true, log to file.  If false, don't log to file.
		*/
		void setEnabled(bool enabled);

		//! Gets the name of the log file.
		//! \return Name of the log file.
		const CString& getFilename() const;

		//! Sets the name of the file to write to.
		//! \param filename Name of the file to write to.
		void setFilename(const CString& filename);

		//! Returns true if we are writing to the console.
		//! \return True if we are writing to the console.
		bool getWriteToConsole() const;

		//! Sets if we are writing to the console.
		//! \param val If true, we should write to the console.
		void setWriteToConsole(bool val);

	private:
		//! If the class is enabled or not.
		bool enabled;

		//! If the class should write to the console or not.
		bool write_to_console;

		//! Filename to write to.
		CString filename;

		//! Application home path.
		CString homepath;

		//! File handle.
		FILE* file;
};

inline
bool CLog::getEnabled() const
{
	return enabled;
}

inline
const CString& CLog::getFilename() const
{
	return filename;
}

inline
void CLog::setEnabled(bool enabled)
{
	this->enabled = enabled;
}

#endif
