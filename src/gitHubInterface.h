// File:  gitHubInterface.h
// Date:  7/8/2016
// Auth:  K. Loux
// Desc:  Class for interfacing with GitHub using their API.

#ifndef GIT_HUB_INTERFACE_H_
#define GIT_HUB_INTERFACE_H_

// Standard C++ headers
#include <vector>

// Local headers
#include "jsonInterface.h"

class GitHubInterface : public JSONInterface
{
public:
	GitHubInterface(const std::string &userAgent);
	bool Initialize(const std::string& user);

	std::vector<std::string> GetUsersRepos();

private:
	// URL building-blocks
	static const std::string apiRoot;

	// JSON tags
	static const std::string userURLTag;

	static const std::string reposURLTag;
	static const std::string nameTag;
	static const std::string repoCountTag;
	static const std::string creationTimeTag;

	std::string userURL;
	std::string reposURLRoot;
};

#endif// GIT_HUB_INTERFACE_H_