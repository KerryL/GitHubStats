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

// Local headers
#include "gitHubInterface.h"

static const std::string userAgent("gitHubStats/1.0");

bool GetGitHubUser(std::string& user)
{
	std::cout << "Enter user:  ";
	std::cin >> user;

	return !user.empty();
}

bool GetGitHubRepo(GitHubInterface& github,
	GitHubInterface::RepoInfo& repo, const std::string& nameToMatch = "")
{
	std::vector<GitHubInterface::RepoInfo> repoList(github.GetUsersRepos());
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

		std::cout << "\nEnter repo:  ";
		std::cin >> repoName;
	}
	else
		repoName = nameToMatch;

	unsigned int i;
	for (i = 0; i < repoList.size(); i++)
	{
		if (repoList[i].name.compare(repoName) == 0)
		{
			repo = repoList[i];
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

void PrintUsage()
{
	std::cout << "Usage:  gitHubStats [user repo]" << std::endl;
	std::cout << "If user and repo names are omitted, user is prompted to enter the names interactively." << std::endl;
}

int main(int argc, char *argv[])
{
	if (argc != 1 && argc != 3)
	{
		PrintUsage();
		return 1;
	}

	std::string user;
	if (argc == 3)
		user = argv[1];
	else if (!GetGitHubUser(user))
		return 1;

	GitHubInterface github(userAgent);
	if (!github.Initialize(user))
		return 1;

	GitHubInterface::RepoInfo repo;
	if (argc == 3)
	{
		if (!GetGitHubRepo(github, repo, argv[2]))
			return 1;
	}
	else
	{
		if (!GetGitHubRepo(github, repo))
			return 1;
	}

	GetStats(github, repo);

	return 0;
}