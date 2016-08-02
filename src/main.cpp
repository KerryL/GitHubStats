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

// Local headers
#include "gitHubInterface.h"
#include "oAuth2Interface.h"

static const std::string userAgent("gitHubStats/1.0");
static const std::string allRepoArg("--all");
static const std::string oAuthFileName("oAuthInfo");

bool GetGitHubUser(std::string& user)
{
	std::cout << "Enter user:  ";
	std::cin >> user;

	return !user.empty();
}

bool GetGitHubRepo(GitHubInterface& github,
	unsigned int& repo, std::vector<GitHubInterface::RepoInfo>& repoList,
	const std::string& nameToMatch = "")
{
	repoList = github.GetUsersRepos();
	if (repoList.size() == 0)
	{
		std::cerr << "Failed to find any repos!" << std::endl;
		return false;
	}

	std::string repoName;
	if (nameToMatch.empty())
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

	if (allRepoArg.compare(repoName) == 0)
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

void PrintReleaseData(const std::vector<GitHubInterface::ReleaseData>& releaseData)
{
	std::cout << "\n" << releaseData.size() << " release(s)" << std::endl;
	unsigned int i, total(0);
	for (i = 0; i < releaseData.size(); i++)
	{
		std::cout << "\n\nTag:      " << releaseData[i].tag;
		std::cout << "\nCreated:  " << releaseData[i].creationTime;

		if (releaseData[i].assets.size() > 0)
		{
			std::cout << "\n" << releaseData[i].assets.size() << " associated file(s)";
			unsigned int j;
			for (j = 0; j < releaseData[i].assets.size(); j++)
			{
				std::cout << "\nFile name:         " << releaseData[i].assets[j].name;
				std::cout << "\n  Size:            " << GetPrettyFileSize(releaseData[i].assets[j].fileSize);
				std::cout << "\n  Download Count:  " << releaseData[i].assets[j].downloadCount << std::endl;
				total += releaseData[i].assets[j].downloadCount;
			}
		}
	}

	std::cout << std::endl;
	std::cout << "Total downloads:  " << total << std::endl;
}

void GetStats(GitHubInterface& github, GitHubInterface::RepoInfo repo)
{
	std::vector<GitHubInterface::ReleaseData> releaseData;
	if (!github.GetRepoData(repo, &releaseData))
		return;

	PrintRepoData(repo);
	PrintReleaseData(releaseData);
}

void GetAllStats(GitHubInterface& github, std::vector<GitHubInterface::RepoInfo>& repoList)
{
	std::vector<std::vector<GitHubInterface::ReleaseData> > releaseData(repoList.size());
	unsigned int i;
	for (i = 0; i < repoList.size(); i++)
		github.GetRepoData(repoList[i], &releaseData[i]);

	const std::string repoNameHeading("Repo Name");
	const std::string dateHeading("Last Modified");
	const std::string languageHeading("Language");
	const std::string releaseCountHeading("Releases");
	const std::string downloadCountHeading("Downloads");

	unsigned int maxNameLen(repoNameHeading.length());
	unsigned int maxLangLen(languageHeading.length());
	unsigned int maxDateLen(dateHeading.length());
	unsigned int maxReleaseCountLen(releaseCountHeading.length());
	unsigned int maxDownloadCountLen(downloadCountHeading.length());

	for (i = 0; i < repoList.size(); i++)
	{
		if (repoList[i].name.length() > maxNameLen)
			maxNameLen = repoList[i].name.length();

		if (repoList[i].language.length() > maxLangLen)
			maxLangLen = repoList[i].language.length();

		if (repoList[i].lastUpdateTime.length() > maxDateLen)
			maxDateLen = repoList[i].lastUpdateTime.length();
	}

	std::cout << std::left << std::setw(maxNameLen) << std::setfill(' ') << repoNameHeading << "  ";
	std::cout << std::left << std::setw(maxDateLen) << std::setfill(' ') << dateHeading << "  ";
	std::cout << std::left << std::setw(maxLangLen) << std::setfill(' ') << languageHeading << "  ";
	std::cout << std::left << std::setw(maxReleaseCountLen) << std::setfill(' ') << releaseCountHeading << "  ";
	std::cout << std::left << std::setw(maxDownloadCountLen) << std::setfill(' ') << downloadCountHeading << "\n";
	std::cout << std::setw(maxNameLen + maxDateLen + maxLangLen
		+ maxReleaseCountLen + maxDownloadCountLen + 8) << std::setfill('-') << "-" << std::endl;

	for (i = 0; i < repoList.size(); i++)
	{
		if (releaseData[i].size() > 0)
		{
			std::cout << std::left << std::setw(maxNameLen) << std::setfill(' ') << repoList[i].name << "  ";
			std::cout << std::left << std::setw(maxDateLen) << std::setfill(' ') << repoList[i].lastUpdateTime << "  ";
			std::cout << std::left << std::setw(maxLangLen) << std::setfill(' ') << repoList[i].language << "  ";
			std::cout << std::left << std::setw(maxReleaseCountLen) << std::setfill(' ') << releaseData[i].size() << "  ";

			unsigned int j, fileCount(0), downloadCount(0);
			for (j = 0; j < releaseData[i].size(); j++)
			{
				fileCount += releaseData[i][j].assets.size();

				unsigned int k;
				for (k = 0; k <	releaseData[i][j].assets.size(); k++)
					downloadCount += releaseData[i][j].assets[k].downloadCount;
			}
			
			
			std::cout << std::left << std::setw(maxDownloadCountLen) << std::setfill(' ') << downloadCount << "\n";
		}
	}
}

void PrintUsage()
{
	std::cout << "Usage:  gitHubStats [user [repo --all]]" << std::endl;
	std::cout << "If user and repo names are omitted, user is prompted\n"
		"to enter the names interactively.  The user name may\n"
		"be specified without any additional arguments, in which\n"
		"case the application expects an interactive response\n"
		"for the repository name.  Instead of a repository name\n"
		"the --all argument may be specified, which prints a\n"
		"table giving the total number of downloads for all of\n"
		"the user's repositories." << std::endl;
}

int main(int argc, char *argv[])
{
	if (argc > 3)
	{
		PrintUsage();
		return 1;
	}

	// Check to see if we need to set up oAuth
	std::ifstream oAuthFile(oAuthFileName.c_str());
	if (oAuthFile.is_open() && oAuthFile.good())
	{
		std::string clientId;
		std::string clientSecret;
		std::string refreshToken;
		oAuthFile >> clientId;
		oAuthFile >> clientSecret;
		oAuthFile >> refreshToken;

		if (!clientId.empty() && !clientSecret.empty())
		{
			OAuth2Interface::Get().SetAuthenticationURL("https://github.com/login/oauth/authorize");
			OAuth2Interface::Get().SetTokenURL("https://github.com/login/oauth/access_token");
			OAuth2Interface::Get().SetScope(" ");// No scope (but it can't be empty due to our implementation) - read only access to public information
			OAuth2Interface::Get().SetClientID(clientId);
			OAuth2Interface::Get().SetClientSecret(clientSecret);
			OAuth2Interface::Get().SetVerboseOutput();
			OAuth2Interface::Get().SetRefreshToken(refreshToken);
			if (OAuth2Interface::Get().GetRefreshToken().compare(refreshToken) != 0)
			{
				oAuthFile.close();
				std::ofstream oAuthFileOut(oAuthFileName.c_str());
				oAuthFileOut << clientId << "\n" << clientSecret << "\n" << refreshToken << std::endl;
			}
		}
	}

	std::string user;
	if (argc != 1)
		user = argv[1];
	else if (!GetGitHubUser(user))
		return 1;

	GitHubInterface github(userAgent);
	if (!github.Initialize(user))
		return 1;

	std::vector<GitHubInterface::RepoInfo> repoList;
	unsigned int repo;
	if (argc == 3)
	{
		if (!GetGitHubRepo(github, repo, repoList, argv[2]))
			return 1;
	}
	else
	{
		if (!GetGitHubRepo(github, repo, repoList))
			return 1;
	}

	if (repo < repoList.size())
		GetStats(github, repoList[repo]);
	else
		GetAllStats(github, repoList);

	return 0;
}
