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
	const std::string& clientId, const std::string& clientSecret)
	: JSONInterface(userAgent), clientId(clientId), clientSecret(clientSecret)
{
}

bool GitHubInterface::Initialize(const std::string& user)
{
	std::string response;
	if (!DoCURLGet(AuthorizeURL(apiRoot), response))
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
	if (!DoCURLGet(AuthorizeURL(userURL), response))
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

	if (!DoCURLGet(AuthorizeURL(reposURL), response))
		return repos;

	root = cJSON_Parse(response.c_str());

	const int count(cJSON_GetArraySize(root));
	int i;
	for (i = 0; i < count; i++)
	{
		cJSON* repo = cJSON_GetArrayItem(root, i);
		repos.push_back(GetRepoData(repo));
	}

	cJSON_Delete(root);

	return repos;
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

bool GitHubInterface::GetRepoData(RepoInfo& info,
	std::vector<ReleaseData>* releaseData)
{
	// For now, no further data added to info, although if we wanted to have a
	// more in-depth look, this is where we should do it.

	if (!releaseData)
		return true;

	releaseData->clear();

	std::string response;
	if (!DoCURLGet(AuthorizeURL(info.releasesURL), response))
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

std::string GitHubInterface::AuthorizeURL(const std::string& url) const
{
	if (clientId.empty() || clientSecret.empty())
		return url;

	return url + "?client_id=" + clientId + "&client_secret=" + clientSecret;
}
