// File:  gitHubInterface.cpp
// Date:  7/8/2016
// Auth:  K. Loux
// Desc:  Class for interfacing with GitHub using their API.

// Local headers
#include "gitHubInterface.h"
#include "cJSON.h"

// cURL headers
#include <curl/curl.h>

// Standard C++ headers
#include <iostream>
#include <sstream>

const std::string GitHubInterface::apiRoot("https://api.github.com/");

const std::string GitHubInterface::userURLTag("user_url");
const std::string GitHubInterface::userReposURLTag("repository_url");

const std::string GitHubInterface::reposURLTag("repos_url");
const std::string GitHubInterface::nameTag("name");
const std::string GitHubInterface::repoCountTag("public_repos");
const std::string GitHubInterface::creationTimeTag("created_at");

const std::string GitHubInterface::descriptionTag("description");
const std::string GitHubInterface::releasesURLTag("releases_url");
const std::string GitHubInterface::updateTimeTag("updated_at");
const std::string GitHubInterface::languageTag("language");

const std::string GitHubInterface::tagNameTag("tag_name");
const std::string GitHubInterface::assetTag("assets");
const std::string GitHubInterface::sizeTag("size");
const std::string GitHubInterface::downloadCountTag("download_count");

GitHubInterface::GitHubInterface(const std::string &userAgent,
	const std::string& token)
	: JSONInterface(userAgent), authData(headerList, token)
{
}

GitHubInterface::~GitHubInterface()
{
	if (headerList)
		curl_slist_free_all(headerList);
}

bool GitHubInterface::Initialize(const std::string& user)
{
	std::string response;
	if (!DoCURLGet(apiRoot, response, &GitHubInterface::AddCurlAuthentication, &authData))
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

	if (ReadJSON(root, userReposURLTag, reposURLRoot))
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

std::vector<GitHubInterface::RepoInfo> GitHubInterface::GetUsersRepos()
{
	std::vector<RepoInfo> repos;

	std::string response;
	if (!DoCURLGet(userURL, response, &GitHubInterface::AddCurlAuthentication, &authData))
		return repos;

	cJSON *root = cJSON_Parse(response.c_str());
	if (!root)
	{
		std::cerr << "Failed to parse returned string (GetUsersRepos())" << std::endl;
		std::cerr << response << std::endl;
		return repos;
	}

	std::string reposURL;
	if (!ReadJSON(root, reposURLTag, reposURL))
	{
		std::cerr << "Failed to find repository access in response" << std::endl;
		return repos;
	}

	cJSON_Delete(root);

	unsigned int page(1);
	while (true)
	{
		if (!DoCURLGet(AppendPageToURL(reposURL, page++), response, &GitHubInterface::AddCurlAuthentication, &authData))
			return repos;

		root = cJSON_Parse(response.c_str());

		const int count(cJSON_GetArraySize(root));
		if (count == 0)
			break;

		for (int i = 0; i < count; i++)
		{
			cJSON* repo = cJSON_GetArrayItem(root, i);
			repos.push_back(GetRepoData(repo));
		}

		cJSON_Delete(root);
	}

	return repos;
}

std::string GitHubInterface::AppendPageToURL(const std::string& root, const unsigned int& page)
{
	std::ostringstream ss;
	ss << root << "?page=" << page;
	return ss.str();
}

GitHubInterface::RepoInfo GitHubInterface::GetRepoData(cJSON* repoNode)
{
	RepoInfo info;

	ReadJSON(repoNode, nameTag, info.name);
	ReadJSON(repoNode, descriptionTag, info.description);
	ReadJSON(repoNode, updateTimeTag, info.lastUpdateTime);
	ReadJSON(repoNode, creationTimeTag, info.creationTime);
	ReadJSON(repoNode, languageTag, info.language);
	if (ReadJSON(repoNode, releasesURLTag, info.releasesURL))
	{
		const std::string idCode("{/id}");
		size_t begin(info.releasesURL.find(idCode));
		if (begin == std::string::npos)
			info.releasesURL.clear();
		else
			info.releasesURL.resize(begin);
	}

	return info;
}

bool GitHubInterface::AddCurlAuthentication(CURL* curl, const ModificationData* data)
{
	/*if (curl_easy_setopt(curl, CURLOPT_HTTPAUTH, CURLAUTH_ANY))
	{
		std::cerr << "Failed to set authentication method\n";
		return false;
	}

	if (curl_easy_setopt(curl, CURLOPT_USERPWD, dynamic_cast<const AuthData*>(data)->basicAuth.c_str()))
	{
		std::cerr << "Failed to set authentication data\n";
		return false;
	}*/

	auto headerList(dynamic_cast<const AuthData*>(data)->headerList);
	headerList = curl_slist_append(headerList, "Accept: application/vnd.github.v3+json");
	if (!headerList)
	{
		std::cerr << "Failed to create accept header\n";
		return false;
	}

	auto token(dynamic_cast<const AuthData*>(data)->token);
	headerList = curl_slist_append(headerList, std::string("Authorization: token " + token).c_str());
	if (!headerList)
	{
		std::cerr << "Failed to create auth header\n";
		return false;
	}

	if (curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headerList))
	{
		std::cerr << "Failed to add header list\n";
		return false;
	}

	return true;
}

bool GitHubInterface::GetRepoData(RepoInfo& info,
	std::vector<ReleaseData>* releaseData)
{
	// For now, no further data added to info, although if we wanted to have a
	// more in-depth look, this is where we should do it.

	if (!releaseData)
		return true;

	releaseData->clear();

	std::string response;
	if (!DoCURLGet(info.releasesURL, response, &GitHubInterface::AddCurlAuthentication, &authData))
		return false;

	cJSON *root = cJSON_Parse(response.c_str());
	if (!root)
	{
		std::cerr << "Failed to parse returned string (GetRepoData())" << std::endl;
		std::cerr << response << std::endl;
		return false;
	}

	const int count(cJSON_GetArraySize(root));
	info.hasReleases = count > 0;
	int i;
	for (i = 0; i < count; i++)
	{
		cJSON* release = cJSON_GetArrayItem(root, i);
		releaseData->push_back(GetReleaseData(release));
	}

	cJSON_Delete(root);

	return true;
}

GitHubInterface::ReleaseData GitHubInterface::GetReleaseData(cJSON* releaseNode)
{
	ReleaseData r;

	ReadJSON(releaseNode, tagNameTag, r.tag);
	ReadJSON(releaseNode, creationTimeTag, r.creationTime);

	cJSON* assetNode = cJSON_GetObjectItem(releaseNode, assetTag.c_str());
	if (!assetNode)
		return r;

	const int count(cJSON_GetArraySize(assetNode));

	int i;
	for (i = 0; i < count; i++)
	{
		cJSON* asset = cJSON_GetArrayItem(assetNode, i);
		r.assets.push_back(GetAssetData(asset));
	}

	return r;
}

GitHubInterface::AssetData GitHubInterface::GetAssetData(cJSON* assetNode)
{
	AssetData info;

	ReadJSON(assetNode, nameTag, info.name);
	ReadJSON(assetNode, sizeTag, info.fileSize);
	ReadJSON(assetNode, downloadCountTag, info.downloadCount);

	return info;
}

bool GitHubInterface::IsBestAsset(const std::string& name)
{
	if (name.length() > 4 && name.substr(name.length() - 4).compare(".exe") == 0)// TODO:  Handle uppercase, too
		return true;
	return false;
}
