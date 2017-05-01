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
	GitHubInterface(const std::string &userAgent,
		const std::string& clientId, const std::string& clientSecret);
	bool Initialize(const std::string& user);

	struct RepoInfo
	{
		std::string name;
		std::string description;
		std::string releasesURL;
		std::string creationTime;
		std::string lastUpdateTime;
		std::string language;

		bool hasReleases;
	};

	struct AssetData
	{
		std::string name;
		unsigned int fileSize;
		unsigned int downloadCount;
	};

	struct ReleaseData
	{
		std::string tag;
		std::string creationTime;
		std::vector<AssetData> assets;
	};

	std::vector<RepoInfo> GetUsersRepos();
	bool GetRepoData(GitHubInterface::RepoInfo& info,
		std::vector<ReleaseData>* releaseData = NULL);

private:
	// URL building-blocks
	static const std::string apiRoot;

	// JSON tags
	static const std::string userURLTag;
	static const std::string userReposURLTag;

	static const std::string reposURLTag;
	static const std::string nameTag;
	static const std::string repoCountTag;
	static const std::string creationTimeTag;

	static const std::string descriptionTag;
	static const std::string releasesURLTag;
	static const std::string updateTimeTag;
	static const std::string languageTag;

	static const std::string tagNameTag;
	static const std::string assetTag;
	static const std::string sizeTag;
	static const std::string downloadCountTag;

	const std::string clientId;
	const std::string clientSecret;

	std::string userURL;
	std::string reposURLRoot;

	std::string AuthorizeURL(const std::string& url) const;

	RepoInfo GetRepoData(cJSON* repoNode);
	ReleaseData GetReleaseData(cJSON* releaseNode);
	AssetData GetAssetData(cJSON* assetNode);
};

#endif// GIT_HUB_INTERFACE_H_
