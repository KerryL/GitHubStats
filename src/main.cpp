// File:  main.cpp
// Date:  7/7/2016
// Auth:  K. Loux
// Desc:  Application entry point for GitHub stats reporter.

// Standard C++ headers
#include <cstdlib>
#include <iostream>

// Local headers
#include "gitHubInterface.h"

bool GetGitHubUser(std::string& user)
{
	std::cout << "Enter user:  ";
	std::cin >> user;

	return !user.empty();
}

bool GetGitHubRepo(GitHubInterface& github, std::string& repo)
{
	std::cout << "Available repos:\n";
	std::vector<std::string> repoList(github.GetUsersRepos());
	unsigned int i;
	for (i = 0; i < repoList.size(); i++)
		std::cout << "  " << repoList[i] << "\n";

	std::cout << "Enter repo:  ";
	std::cin >> repo;

	// TODO:  Ensure specified repo is in the list

	return !repo.empty();
}

void GetStats(GitHubInterface& github, const std::string& repo)
{
}

#include <fstream>
#include <cassert>
int main()
{
	std::string user, repo;
	if (!GetGitHubUser(user))
		return 1;

	GitHubInterface github;
	github.SetVerboseOutput();
	github.SetCACertificatePath("../");
	if (!github.Initialize(user))
		return 1;

	if (!GetGitHubRepo(github, repo))
		return 1;

	GetStats(github, repo);

	return 0;
}