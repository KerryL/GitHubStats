// File:  jsonInterface.cpp
// Date:  7/7/2016
// Auth:  K. Loux
// Desc:  Class for interfacing with a remote server using JSON and CURL.

// Standard C++ headers
#include <cstdlib>
#include <cassert>
#include <iostream>
#include <sstream>
#include <ctime>
#include <algorithm>

// Standard C headers
#include <string.h>

// *nix headers
#ifndef _WIN32
#include <unistd.h>
#endif

// cURL headers
#include <curl/curl.h>

// Local headers
#include "jsonInterface.h"
#include "cJSON.h"

//==========================================================================
// Class:			JSONInterface
// Function:		JSONInterface
//
// Description:		Constructor for JSONInterface class.
//
// Input Arguments:
//		None
//
// Output Arguments:
//		None
//
// Return Value:
//		None
//
//==========================================================================
JSONInterface::JSONInterface(const std::string& userAgent) : userAgent(userAgent)
{
	verbose = false;
}

//==========================================================================
// Class:			JSONInterface
// Function:		DoCURLPost
//
// Description:		Creates a cURL object, POSTs, obtains response, and cleans up.
//
// Input Arguments:
//		url		= const std::string&
//		data	= const std::string&
//
// Output Arguments:
//		response	= std::string&
//
// Return Value:
//		bool, true for success, false otherwise
//
//==========================================================================
bool JSONInterface::DoCURLPost(const std::string &url, const std::string &data,
	std::string &response) const
{
	CURL *curl = curl_easy_init();
	if (!curl)
	{
		std::cerr << "Failed to initialize CURL" << std::endl;
		return false;
	}

	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, JSONInterface::CURLWriteCallback);
	response.clear();
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
	curl_easy_setopt(curl, CURLOPT_USE_SSL, CURLUSESSL_ALL);

	if (!caCertificatePath.empty())
		curl_easy_setopt(curl, CURLOPT_CAPATH, caCertificatePath.c_str());

	if (verbose)
		curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);

	if (!userAgent.empty())
		curl_easy_setopt(curl, CURLOPT_USERAGENT, userAgent.c_str());

	curl_easy_setopt(curl, CURLOPT_POST, true);
/*	char *urlEncodedData = curl_easy_escape(curl, data.c_str(), data.length());
	if (!urlEncodedData)
	{
		std::cerr << "Failed to url-encode the data" << std::endl;
		curl_easy_cleanup(curl);
		return false;
	}
	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, urlEncodedData);
	curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, strlen(urlEncodedData));*/

	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data.c_str());
	curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, data.length());

	curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
	CURLcode result = curl_easy_perform(curl);

//	curl_free(urlEncodedData);
	if(result != CURLE_OK)
	{
		std::cerr << "Failed issuing https POST:  " << curl_easy_strerror(result) << "." << std::endl;
		curl_easy_cleanup(curl);
		return false;
	}

	curl_easy_cleanup(curl);
	return true;
}

//==========================================================================
// Class:			JSONInterface
// Function:		DoCURLGet
//
// Description:		Creates a cURL object, GETs, obtains response, and cleans up.
//
// Input Arguments:
//		url		= const std::string&
//
// Output Arguments:
//		response	= std::string&
//
// Return Value:
//		bool, true for success, false otherwise
//
//==========================================================================
bool JSONInterface::DoCURLGet(const std::string &url, std::string &response) const
{
	CURL *curl = curl_easy_init();
	if (!curl)
	{
		std::cerr << "Failed to initialize CURL" << std::endl;
		return false;
	}

	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, JSONInterface::CURLWriteCallback);
	response.clear();
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
	//curl_easy_setopt(curl, CURLOPT_USE_SSL, CURLUSESSL_ALL);

	if (!caCertificatePath.empty())
		curl_easy_setopt(curl, CURLOPT_CAPATH, caCertificatePath.c_str());
	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);// TODO:  Remove this option

	if (!userAgent.empty())
		curl_easy_setopt(curl, CURLOPT_USERAGENT, userAgent.c_str());

	if (verbose)
		curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);

	curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
	CURLcode result = curl_easy_perform(curl);

	if(result != CURLE_OK)
	{
		std::cerr << "Failed issuing HTTP(S) GET:  " << curl_easy_strerror(result) << "." << std::endl;
		curl_easy_cleanup(curl);
		return false;
	}

	curl_easy_cleanup(curl);
	return true;
}

//==========================================================================
// Class:			JSONInterface
// Function:		CURLWriteCallback
//
// Description:		Static member function for receiving returned data from cURL.
//
// Input Arguments:
//		ptr			= char*
//		size		= size_t indicating number of elements of size nmemb
//		nmemb		= size_t indicating size of each element
//		userData	= void* (must be pointer to std::string)
//
// Output Arguments:
//		None
//
// Return Value:
//		size_t indicating number of bytes read
//
//==========================================================================
size_t JSONInterface::CURLWriteCallback(char *ptr, size_t size, size_t nmemb, void *userData)
{
	size_t totalSize = size * nmemb;
//	((std::string*)userData)->clear();
	((std::string*)userData)->append(ptr, totalSize);

	return totalSize;
}

//==========================================================================
// Class:			JSONInterface
// Function:		ReadJSON
//
// Description:		Reads the specified field from the JSON array.
//
// Input Arguments:
//		root	= cJSON*
//		field	= const std::string&
//
// Output Arguments:
//		value	= int&
//
// Return Value:
//		bool, true for success, false otherwise
//
//==========================================================================
bool JSONInterface::ReadJSON(cJSON *root, const std::string& field, int &value) const
{
	cJSON *element = cJSON_GetObjectItem(root, field.c_str());
	if (!element)
	{
		//std::cerr << "Failed to read field '" << field << "' from JSON array" << std::endl;
		return false;
	}

	value = element->valueint;

	return true;
}

//==========================================================================
// Class:			JSONInterface
// Function:		ReadJSON
//
// Description:		Reads the specified field from the JSON array.
//
// Input Arguments:
//		root	= cJSON*
//		field	= const std::string&
//
// Output Arguments:
//		value	= std::string&
//
// Return Value:
//		bool, true for success, false otherwise
//
//==========================================================================
bool JSONInterface::ReadJSON(cJSON *root, const std::string& field, std::string &value) const
{
	cJSON *element = cJSON_GetObjectItem(root, field.c_str());
	if (!element)
	{
		//std::cerr << "Failed to read field '" << field << "' from JSON array" << std::endl;
		return false;
	}

	value = element->valuestring;

	return true;
}

//==========================================================================
// Class:			JSONInterface
// Function:		ReadJSON
//
// Description:		Reads the specified field from the JSON array.
//
// Input Arguments:
//		root	= cJSON*
//		field	= const std::string&
//
// Output Arguments:
//		value	= double&
//
// Return Value:
//		bool, true for success, false otherwise
//
//==========================================================================
bool JSONInterface::ReadJSON(cJSON *root, const std::string& field, double &value) const
{
	cJSON *element = cJSON_GetObjectItem(root, field.c_str());
	if (!element)
	{
		//std::cerr << "Failed to read field '" << field << "' from JSON array" << std::endl;
		return false;
	}

	value = element->valuedouble;

	return true;
}
