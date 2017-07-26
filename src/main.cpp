// File:  main.cpp
// Date:  7/7/2016
// Auth:  K. Loux
// Desc:  Application entry point for GitHub stats reporter.

// Standard C++ headers
#include <cstdlib>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <cmath>
#include <fstream>
#include <map>

// Local headers
#include "gitHubInterface.h"

static const std::string userAgent("gitHubStats/1.0");
static const std::string oAuthFileName("oAuthInfo");
static const std::string lastCountsFileName("lastCounts");

bool GetGitHubUser(std::string& user)
{
	std::cout << "Enter user:  ";
	std::cin >> user;

	return !user.empty();
}

bool GetGitHubRepo(GitHubInterface& github,
	unsigned int& repo, std::vector<GitHubInterface::RepoInfo>& repoList,
	const bool& allRepos, const std::string& nameToMatch = "")
{
	repoList = github.GetUsersRepos();
	if (repoList.size() == 0)
	{
		std::cerr << "Failed to find any repos!" << std::endl;
		return false;
	}

	const std::string allRepoArg("--all");
	std::string repoName;
	if (nameToMatch.empty() && !allRepos)
	{
		std::cout << "\nFound " << repoList.size() << " repos:\n";
		unsigned int i, maxNameLen(0);
		for (i = 0; i < repoList.size(); i++)
		{
			if (repoList[i].name.length() > maxNameLen)
				maxNameLen = repoList[i].name.length();
		}

		for (i = 0; i < repoList.size(); i++)
		{
			std::cout << "  " << std::left << std::setw(maxNameLen) << std::setfill(' ') << repoList[i].name;
			std::cout << "    " << repoList[i].description;
			std::cout << "\n";
		}

		std::cout << "\nEnter repo (or " << allRepoArg << "):  ";
		std::cin >> repoName;
	}
	else
		repoName = nameToMatch;

	if (allRepoArg.compare(repoName) == 0 || allRepos)
	{
		repo = repoList.size();
		return true;
	}

	unsigned int i;
	for (i = 0; i < repoList.size(); i++)
	{
		if (repoList[i].name.compare(repoName) == 0)
		{
			repo = i;
			return true;
		}
	}

	return false;
}

void PrintRepoData(const GitHubInterface::RepoInfo& repoData)
{
	std::cout << "\nProject:      " << repoData.name;
	std::cout << "\nDescription:  " << repoData.description;
	std::cout << "\nLanguage:     " << repoData.language;
	std::cout << "\nCreated:      " << repoData.creationTime;
	std::cout << "\nUpdated:      " << repoData.lastUpdateTime << std::endl;
}

std::string GetPrettyFileSize(const unsigned int& bytes)
{
	std::ostringstream ss;

	const double threshold(0.75);
	const unsigned int rollover(1024);
	if (bytes < rollover * rollover)
		ss << bytes << " bytes";
	else
	{
		ss.precision(2);
		if (bytes < pow(rollover, 2) * threshold)
			ss << std::fixed << bytes / rollover << " kB";
		else if (bytes < pow(rollover, 3) * threshold)
			ss << std::fixed << bytes / pow(rollover, 2) << " MB";
		else if (bytes < pow(rollover, 4) * threshold)
			ss << std::fixed << bytes / pow(rollover, 4) << " GB";
		else// if (bytes < pow(rollover, 5) * threshold)
			ss << std::fixed << bytes / pow(rollover, 5) << " TB";
	}

	return ss.str();
}

typedef std::map<std::string, unsigned int> AssetDownloadCountMap;
struct TagDownloadCountMap// Name is too long (generates C4503) if we use a typedef here
{
	std::map<std::string, AssetDownloadCountMap> assetCountMap;
};
typedef std::map<std::string, TagDownloadCountMap> RepoTagInfoMap;

bool ReadLastCountData(RepoTagInfoMap& data)
{
	std::ifstream file(lastCountsFileName.c_str());
	if (!file.is_open() || !file.good())
	{
		std::cerr << "Failed to open '" << lastCountsFileName << "' for input\n";
		return false;
	}

	unsigned int i, repoCount;
	if (!(file >> repoCount).good())
	{
		std::cerr << "Failed to read repository count\n";
		return false;
	}

	for (i = 0; i < repoCount; ++i)
	{
		std::string repoName;
		file >> repoName;

		unsigned int j, releaseCount;
		if (!(file >> releaseCount).good())
		{
			std::cerr << "Failed to read release count for repository '" << repoName << "\n";
			return false;
		}

		for (j = 0; j < releaseCount; ++j)
		{
			std::string releaseTag;
			file >> releaseTag;

			unsigned int k, assetCount;
			if (!(file >> assetCount).good())
			{
				std::cerr << "Failed to read asset count for release '" << releaseTag << "' in repository '" << repoName << "\n";
				return false;
			}

			for (k = 0; k < assetCount; ++k)
			{
				std::string assetName;
				file >> assetName;
				
				unsigned int count;
				if (!(file >> count).good())
				{
					std::cerr << "Failed to read download count for asset '" << assetName
						<< "' from release '" << releaseTag << "' in repository '" << repoName << "\n";
					return false;
				}

				data[repoName].assetCountMap[releaseTag][assetName] = count;
			}
		}
	}

	return true;
}

bool WriteLastCountData(const RepoTagInfoMap& data)
{
	std::ofstream file(lastCountsFileName.c_str());
	if (!file.is_open() || !file.good())
	{
		std::cerr << "Failed to open '" << lastCountsFileName << "' for output\n";
		return false;
	}

	file << data.size() << '\n';
	for (const auto& repoIter : data)
	{
		file << repoIter.first << '\n';
		file << repoIter.second.assetCountMap.size() << '\n';

		for (const auto& releaseIter : repoIter.second.assetCountMap)
		{
			file << releaseIter.first << '\n';
			file << releaseIter.second.size() << '\n';

			for (const auto& assetIter : releaseIter.second)
			{
				file << assetIter.first << '\n';
				file << assetIter.second << '\n';
			}
		}
	}

	file << std::endl;

	return true;
}

void PrintReleaseData(const std::vector<GitHubInterface::ReleaseData>& releaseData, const std::string& repoName, const bool& compare)
{
	RepoTagInfoMap downloadData;
	if (compare && !ReadLastCountData(downloadData))
		std::cerr << "Failed to read comparison data; assuming zero previous downloads\n";

	std::cout << "\n" << releaseData.size() << " release(s)" << std::endl;
	unsigned int total(0), totalDelta(0);
	for (const auto& release : releaseData)
	{
		std::cout << "\n\nTag:      " << release.tag;
		std::cout << "\nCreated:  " << release.creationTime;

		if (release.assets.size() > 0)
		{
			std::cout << "\n" << release.assets.size() << " associated file(s)";
			for (const auto& asset : release.assets)
			{
				std::cout << "\nFile name:         " << asset.name;
				std::cout << "\n  Size:            " << GetPrettyFileSize(asset.fileSize);
				std::cout << "\n  Download Count:  " << asset.downloadCount;

				if (compare)
				{
					const unsigned int lastCount([downloadData, repoName, release, asset]()
					{
						auto repoIter(downloadData.find(repoName));
						if (repoIter == downloadData.end())
							return 0U;

						auto releaseIter(repoIter->second.assetCountMap.find(release.tag));
						if (releaseIter == repoIter->second.assetCountMap.end())
							return 0U;

						auto assetIter(releaseIter->second.find(asset.name));
						if (assetIter == releaseIter->second.end())
							return 0U;

						return assetIter->second;
					}());
					const unsigned int delta(asset.downloadCount - lastCount);
					totalDelta += delta;
					if (delta > 0)
						std::cout << " (+" << delta << ")";
					downloadData[repoName].assetCountMap[release.tag][asset.name] = asset.downloadCount;
				}

				std::cout << std::endl;
				total += asset.downloadCount;
			}
		}
	}

	std::cout << std::endl;
	std::cout << "Total downloads:  " << total;

	if (compare && totalDelta > 0)
		std::cout << " (+" << totalDelta << ")";

	std::cout << std::endl;

	if (compare)
		WriteLastCountData(downloadData);
}

void GetStats(GitHubInterface& github, GitHubInterface::RepoInfo repo, const bool& compare)
{
	std::vector<GitHubInterface::ReleaseData> releaseData;
	if (!github.GetRepoData(repo, &releaseData))
		return;

	PrintRepoData(repo);
	PrintReleaseData(releaseData, repo.name, compare);
}

void GetAllStats(GitHubInterface& github, std::vector<GitHubInterface::RepoInfo>& repoList, const bool& compare)
{
	std::vector<std::vector<GitHubInterface::ReleaseData> > releaseData(repoList.size());
	unsigned int i;
	for (i = 0; i < repoList.size(); i++)
		github.GetRepoData(repoList[i], &releaseData[i]);

	const std::string repoNameHeading("Repo Name");
	const std::string dateHeading("Last Modified");
	const std::string languageHeading("Language");
	const std::string releaseCountHeading("Releases");
	const std::string totalDownloadCountHeading("Total");
	const std::string latestDownloadCountHeading("Latest");
	const std::string deltaCountHeading("Delta");

	unsigned int maxNameLen(repoNameHeading.length());
	unsigned int maxLangLen(languageHeading.length());
	unsigned int maxDateLen(dateHeading.length());
	unsigned int maxReleaseCountLen(releaseCountHeading.length());
	unsigned int maxTotalDownloadCountLen(totalDownloadCountHeading.length());
	unsigned int maxLatestDownloadCountLen(latestDownloadCountHeading.length());
	unsigned int maxDeltaCountLen(deltaCountHeading.length());

	for (const auto& repo : repoList)
	{
		if (!repo.hasReleases)
			continue;

		if (repo.name.length() > maxNameLen)
			maxNameLen = repo.name.length();

		if (repo.language.length() > maxLangLen)
			maxLangLen = repo.language.length();

		if (repo.lastUpdateTime.length() > maxDateLen)
			maxDateLen = repo.lastUpdateTime.length();
	}

	std::cout << std::left << std::setw(maxNameLen)
		<< std::setfill(' ') << repoNameHeading << "  ";
	std::cout << std::left << std::setw(maxDateLen)
		<< std::setfill(' ') << dateHeading << "  ";
	std::cout << std::left << std::setw(maxLangLen)
		<< std::setfill(' ') << languageHeading << "  ";
	std::cout << std::left << std::setw(maxReleaseCountLen)
		<< std::setfill(' ') << releaseCountHeading << "  ";
	std::cout << std::left << std::setw(maxTotalDownloadCountLen)
		<< std::setfill(' ') << totalDownloadCountHeading << "  ";
	std::cout << std::left << std::setw(maxLatestDownloadCountLen)
		<< std::setfill(' ') << latestDownloadCountHeading;

	int width(maxNameLen + maxDateLen + maxLangLen
		+ maxReleaseCountLen + maxTotalDownloadCountLen
		+ maxLatestDownloadCountLen + 10);
	if (compare)
	{
		std::cout << "  " << std::left << std::setw(maxDeltaCountLen)
			<< std::setfill(' ') << deltaCountHeading;
		width += 2 + maxDeltaCountLen;
	}
	std::cout << '\n';

	std::cout << std::setw(width)
		<< std::setfill('-') << '-' << std::endl;

	RepoTagInfoMap downloadData;
	if (compare && !ReadLastCountData(downloadData))
		std::cerr << "Failed to read comparison data; assuming zero previous downloads\n";

	for (i = 0; i < repoList.size(); i++)
	{
		if (releaseData[i].size() > 0)
		{
			std::cout << std::left << std::setw(maxNameLen) << std::setfill(' ') << repoList[i].name << "  ";
			std::cout << std::left << std::setw(maxDateLen) << std::setfill(' ') << repoList[i].lastUpdateTime << "  ";
			std::cout << std::left << std::setw(maxLangLen) << std::setfill(' ') << repoList[i].language << "  ";
			std::cout << std::left << std::setw(maxReleaseCountLen) << std::setfill(' ') << releaseData[i].size() << "  ";

			auto repoCountIter(downloadData.find(repoList[i].name));
			unsigned int lastDownloadCount(0);
			if (repoCountIter != downloadData.end())
			{
				for (const auto& releaseCountIter : repoCountIter->second.assetCountMap)
				{
					const auto* bestAsset(&*releaseCountIter.second.begin());
					for (const auto& assetCountIter : releaseCountIter.second)
					{
						if (GitHubInterface::IsBestAsset(assetCountIter.first))
							bestAsset = &assetCountIter;
					}

					lastDownloadCount += bestAsset->second;
				}
			}

			unsigned int fileCount(0), totalDownloadCount(0), latestDownloadCount(0);
			time_t latestRelease(0);
			for (const auto& release : releaseData[i])
			{
				fileCount += release.assets.size();
				std::istringstream dateSS(release.creationTime);
				struct std::tm tm;
				dateSS >> std::get_time(&tm, "%Y-%m-%dT%X");
				const time_t releaseDate(std::mktime(&tm));

				// Best asset is the one that is an executable (or just take the first one)
				auto bestAsset = release.assets.front();
				for (const auto& asset : release.assets)
				{
					if (GitHubInterface::IsBestAsset(asset.name))
						bestAsset = asset;
				}

				totalDownloadCount += bestAsset.downloadCount;
				downloadData[repoList[i].name].assetCountMap[release.tag][bestAsset.name] = bestAsset.downloadCount;

				if (difftime(latestRelease, releaseDate) < 0.0)
				{
					latestDownloadCount = bestAsset.downloadCount;
					latestRelease = releaseDate;
				}
			}
			
			std::cout << std::left << std::setw(maxTotalDownloadCountLen) << std::setfill(' ') << totalDownloadCount << "  ";
			std::cout << std::left << std::setw(maxLatestDownloadCountLen) << std::setfill(' ') << latestDownloadCount;

			if (compare)
			{
				std::cout << "  " << std::left << std::setw(maxDeltaCountLen) << std::setfill(' ');
				const int deltaDownloadCount(totalDownloadCount - lastDownloadCount);
				if (deltaDownloadCount > 0)
					std::cout << std::showpos << deltaDownloadCount << std::noshowpos;
				else
					std::cout << deltaDownloadCount;
			}

			std::cout << '\n';
		}
	}

	if (compare)
		WriteLastCountData(downloadData);
}

void PrintUsage(const std::string& appName)
{
	std::cout << "Usage:  " << appName << " [--compare] [user [repo --all]]" << std::endl;
	std::cout << "If user and repo names are omitted, user is prompted\n"
		"to enter the names interactively.  The user name may\n"
		"be specified without any additional arguments, in which\n"
		"case the application expects an interactive response\n"
		"for the repository name.  Instead of a repository name\n"
		"the --all argument may be specified, which prints a\n"
		"table giving the total number of downloads for all of\n"
		"the user's repositories.\n\nThe --compare option\n"
		"compares the number of downloads reported with the\n"
		"number of downloads reported last time the repo was\n"
		"polled.  Current download count is stored in a local\n"
		"file." << std::endl;
}

struct CmdLineArgs
{
	bool compare = false;
	std::string user;
	std::string repo;
	bool allRepos = false;
};

bool ProcessArguments(int argc, char *argv[], CmdLineArgs& args)
{
	if (argc > 4)
		return false;

	const std::string compareArg("--compare");
	const std::string allArg("--all");

	bool expectRepo(false);

	int i;
	for (i = 1; i < argc; ++i)
	{
		if (compareArg.compare(argv[i]) == 0)
		{
			args.compare = true;
			expectRepo = false;
		}
		else if (allArg.compare(argv[i]) == 0)
		{
			args.allRepos = true;
			expectRepo = false;
		}
		else if (args.user.empty())
		{
			args.user = argv[i];
			expectRepo = true;
		}
		else if (expectRepo)
		{
			args.repo = argv[i];
			expectRepo = false;
		}
		else
		{
			std::cerr << "Unexpected argument:  '" << argv[i] << "'\n";
			return false;
		}
	}

	return true;
}

int main(int argc, char *argv[])
{
	CmdLineArgs args;
	if (!ProcessArguments(argc, argv, args))
	{
		PrintUsage(argv[0]);
		return 1;
	}

	if (args.user.empty())
	{
		if (!GetGitHubUser(args.user))
			return 1;
	}

	// Check to see if we need to set up oAuth
	std::string clientId;
	std::string clientSecret;
	std::ifstream oAuthFile(oAuthFileName.c_str());
	if (oAuthFile.is_open() && oAuthFile.good())
	{
		oAuthFile >> clientId;
		oAuthFile >> clientSecret;
	}

	GitHubInterface github(userAgent, clientId, clientSecret);
	if (!github.Initialize(args.user))
		return 1;

	std::vector<GitHubInterface::RepoInfo> repoList;
	unsigned int repo;
	if (args.repo.empty())
	{
		if (!GetGitHubRepo(github, repo, repoList, args.allRepos))
			return 1;
	}
	else
	{
		if (!GetGitHubRepo(github, repo, repoList, args.allRepos, args.repo))
			return 1;
	}

	if (repo < repoList.size())
		GetStats(github, repoList[repo], args.compare);
	else
		GetAllStats(github, repoList, args.compare);

	return 0;
}
