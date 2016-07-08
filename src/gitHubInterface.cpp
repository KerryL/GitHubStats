// File:  gitHubInterface.cpp
// Date:  7/8/2016
// Auth:  K. Loux
// Desc:  Class for interfacing with GitHub using their API.

// Standard C++ headers
#include <iostream>

// Local headers
#include "gitHubInterface.h"
#include "cJSON.h"

const std::string GitHubInterface::apiRoot("https://api.github.com/");

const std::string GitHubInterface::userURLTag("user_url");

const std::string GitHubInterface::reposURLTag("repository_url");
const std::string GitHubInterface::nameTag("name");
const std::string GitHubInterface::repoCountTag("public_repos");
const std::string GitHubInterface::creationTimeTag("created_at");

GitHubInterface::GitHubInterface(const std::string &userAgent) : JSONInterface(userAgent)
{
}

bool GitHubInterface::Initialize(const std::string& user)
{
	std::string response;
	if (!DoCURLGet(apiRoot, response))
		return false;

	cJSON *root = cJSON_Parse(response.c_str());
	if (!root)
	{
		std::cerr << "Failed to parse returned string (Initialize())" << std::endl;
		std::cerr << response << std::endl;
		return false;
	}

	userURL.clear();
	reposURLRoot.clear();

	if (ReadJSON(root, userURLTag, userURL))
	{
		const std::string userCode("{user}");
		size_t begin(userURL.find(userCode));
		if (begin == std::string::npos)
			userURL.clear();
		else
			userURL.replace(begin, userCode.length(), user);
	}

	if (ReadJSON(root, reposURLTag, reposURLRoot))
	{
		const std::string ownerCode("{owner}");
		size_t begin(reposURLRoot.find(ownerCode));
		if (begin == std::string::npos)
			reposURLRoot.clear();
		else
		{
			reposURLRoot.replace(begin, ownerCode.length(), user);
			const std::string repoCode("{repo}");
			size_t end(reposURLRoot.find(repoCode));
			if (end == std::string::npos)
				reposURLRoot.clear();
			else
				reposURLRoot.resize(end);
		}
	}

	cJSON_Delete(root);

	return !userURL.empty() && !reposURLRoot.empty();
}

std::vector<std::string> GitHubInterface::GetUsersRepos()
{
	std::vector<std::string> repos;
	return repos;
}
