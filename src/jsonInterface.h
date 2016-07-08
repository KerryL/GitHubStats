// File:  jsonInterface.h
// Date:  7/7/2016
// Auth:  K. Loux
// Desc:  Class for interfacing with a remote server using JSON and CURL.

#ifndef JSON_INTERFACE_H_
#define JSON_INTERFACE_H_

// Standard C++ headers
#include <string>

// cJSON forward declarations
struct cJSON;

class JSONInterface
{
public:
	JSONInterface();
	virtual ~JSONInterface() {}

	void SetCACertificatePath(const std::string& path) { caCertificatePath = path; }
	void SetVerboseOutput(const bool& verboseOutput = true) { verbose = verboseOutput; }

protected:
	std::string caCertificatePath;
	bool verbose;

	bool DoCURLPost(const std::string &url, const std::string &data,
		std::string &response) const;
	bool DoCURLGet(const std::string &url, std::string &response) const;

	bool ReadJSON(cJSON *root, const std::string& field, int &value) const;
	bool ReadJSON(cJSON *root, const std::string& field, std::string &value) const;
	bool ReadJSON(cJSON *root, const std::string& field, double &value) const;

	static size_t CURLWriteCallback(char *ptr, size_t size, size_t nmemb, void *userData);
};

#endif// JSON_INTERFACE_H_