// File:  oAuth2Interface.cpp
// Date:  4/15/2013
// Auth:  K. Loux
// Desc:  Handles interface to a server using OAuth 2.0.

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
#include "oAuth2Interface.h"
#include "cJSON.h"

//==========================================================================
// Class:			OAuth2Interface
// Function:		OAuth2Interface
//
// Description:		Constant declarations
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
OAuth2Interface *OAuth2Interface::singleton = NULL;

//==========================================================================
// Class:			OAuth2Interface
// Function:		OAuth2Interface
//
// Description:		Constructor for OAuth2Interface class.
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
OAuth2Interface::OAuth2Interface()
{
	verbose = false;
}

//==========================================================================
// Class:			OAuth2Interface
// Function:		~OAuth2Interface
//
// Description:		Destructor for OAuth2Interface class.
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
OAuth2Interface::~OAuth2Interface()
{
}

//==========================================================================
// Class:			OAuth2Interface
// Function:		Get (static)
//
// Description:		Access method for singleton pattern.
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
OAuth2Interface& OAuth2Interface::Get()
{
	if (!singleton)
		singleton = new OAuth2Interface;

	return *singleton;
}

//==========================================================================
// Class:			OAuth2Interface
// Function:		Destroy (static)
//
// Description:		Cleans up memory associated with singleton pattern.
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
void OAuth2Interface::Destroy()
{
	if (singleton)
		delete singleton;

	singleton = NULL;
}

//==========================================================================
// Class:			OAuth2Interface
// Function:		SetRefreshToken
//
// Description:		Sets the refresh token.  If the token is not valid, request
//					information from the user to obtain a valid token.
//
// Input Arguments:
//		refreshToken	= const std::string&
//
// Output Arguments:
//		None
//
// Return Value:
//		None
//
//==========================================================================
void OAuth2Interface::SetRefreshToken(const std::string &refreshTokenIn)
{
	// If the token isn't valid, request one, otherwise, use it as-is
	if (refreshTokenIn.length() < 2)// TODO:  Better way to tell if it's valid?
		refreshToken = RequestRefreshToken();// TODO:  Check for errors (returned empty string?)
	else
		refreshToken = refreshTokenIn;
}

//==========================================================================
// Class:			OAuth2Interface
// Function:		RequestRefreshToken
//
// Description:		Requests a refresh token from the server.
//
// Input Arguments:
//		None
//
// Output Arguments:
//		None
//
// Return Value:
//		std::string containing refresh token, or emtpy string on error
//
//==========================================================================
std::string OAuth2Interface::RequestRefreshToken()
{
	assert(!authURL.empty() &&
		!tokenURL.empty());

	if (IsLimitedInput())
	{
		std::string readBuffer;
		if (!DoCURLPost(authURL, AssembleRefreshRequestQueryString(), readBuffer))
			return "";

		if (ResponseContainsError(readBuffer))
			return "";

		AuthorizationResponse authResponse;
		if (!HandleAuthorizationRequestResponse(readBuffer, authResponse))
			return "";

		std::string queryString = AssembleAccessRequestQueryString(authResponse.deviceCode);

		time_t startTime = time(NULL);
		time_t now = startTime;
		while (!HandleRefreshRequestResponse(readBuffer, true))
		{
#ifdef _WIN32
			Sleep(1000 * authResponse.interval);
#else
			sleep(authResponse.interval);
#endif
			now = time(NULL);
			if (difftime(now, startTime) > authResponse.expiresIn)
			{
				std::cerr << "Request timed out - restart application to start again" << std::endl;
				return "";
			}

			if (!DoCURLPost(tokenURL, queryString, readBuffer))
				return "";

			if (ResponseContainsError(readBuffer))
				return "";
		}
	}
	else
	{
		assert(!responseType.empty());

		std::string stateKey = "";//GenerateSecurityStateKey();// Not sure why it doesn't work with the state key...
		std::string readBuffer;
		
		// TODO:  Alternatively, we can pop up a browser, wait for the user
		// to verify permissions, then grab the result ourselves, without
		// requiring the user to copy/paste into browser, then again to this
		// application.
		// The benefit of doing it the way we're doing it now, though, is
		// that the browser used to authenticate does not need to be on the
		// same machine that is running this application.
		std::cout << "Enter this address in your browser:" << std::endl
			<< authURL << "?" << AssembleRefreshRequestQueryString(stateKey) << std::endl;

		std::string authorizationCode;
		if (RedirectURIIsLocal())
		{
			// TODO:  What now?  Need to receive authorization code by listening to appropriate port?
			// Can cURL do this?
			//curl_easy_setopt(curl, CURLOPT_PORT, StripPortFromLocalRedirectURI());
			assert(false);
		}
		else
		{
			// TODO:  Grab verification code automatically (see note above
			// prior to "Enter this address...")
			std::cout << "Enter verification code:" << std::endl;
			std::cin >> authorizationCode;
		}

		if (!DoCURLPost(tokenURL, AssembleAccessRequestQueryString(authorizationCode), readBuffer) ||
			ResponseContainsError(readBuffer) ||
			!HandleRefreshRequestResponse(readBuffer))
		{
			std::cerr << "Failed to obtain refresh token" << std::endl;
			return "";
		}
	}

	std::cout << "Successfully obtained refresh token" << std::endl;
	return refreshToken;
}

//==========================================================================
// Class:			OAuth2Interface
// Function:		ResponseContainsError
//
// Description:		Checks JSON array to see if there is an error entry.
//					"Authorization pending" errors are not considered errors.
//
// Input Arguments:
//		buffer		= const std::string & containing JSON string
//
// Output Arguments:
//		None
//
// Return Value:
//		bool, true for error, false otherwise
//
//==========================================================================
bool OAuth2Interface::ResponseContainsError(const std::string &buffer)
{
	cJSON *root = cJSON_Parse(buffer.c_str());
	if (!root)
	{
		std::cerr << "Failed to parse returned string (ResponseContainsError())" << std::endl;
		if (verbose)
			std::cerr << buffer << std::endl;
		return true;
	}

	std::string error;
	if (ReadJSON(root, "error", error))
	{
		if (error.compare("authorization_pending") != 0)
		{
			std::cerr << "Recieved error from OAuth server:  " << error;
			std::string description;
			if (ReadJSON(root, "error_description", description))
				std::cerr << " - " << description;
			std::cerr << std::endl;
			cJSON_Delete(root);
			return true;
		}

		cJSON_Delete(root);
	}

	return false;
}

//==========================================================================
// Class:			OAuth2Interface
// Function:		HandleAuthorizationRequestResponse
//
// Description:		Processes JSON responses from server.  Used for input-limited
//					devices only.
//
// Input Arguments:
//		buffer		= const std::string & containing JSON string
//
// Output Arguments:
//		response	= AuthorizationResponse&
//
// Return Value:
//		bool, true for success, false otherwise
//
//==========================================================================
bool OAuth2Interface::HandleAuthorizationRequestResponse(
	const std::string &buffer, AuthorizationResponse &response)
{
	assert(IsLimitedInput());

	cJSON *root = cJSON_Parse(buffer.c_str());
	if (!root)
	{
		std::cerr << "Failed to parse returned string (HandleAuthorizationRequestResponse())" << std::endl;
		return false;
	}

	std::string userCode, verificationURL;

	// TODO:  Check state key?
	if (!ReadJSON(root, "device_code", response.deviceCode) ||
		!ReadJSON(root, "user_code", userCode) ||
		!ReadJSON(root, "verification_url", verificationURL) ||
		!ReadJSON(root, "expires_in", response.expiresIn) ||
		!ReadJSON(root, "interval", response.interval))
	{
		cJSON_Delete(root);
		return false;
	}

	std::cout << "Please visit this URL: " << std::endl << verificationURL << std::endl;
	std::cout << "And enter this code (case sensitive):" << std::endl << userCode << std::endl;

	cJSON_Delete(root);
	return true;
}

//==========================================================================
// Class:			OAuth2Interface
// Function:		HandleRefreshRequestResponse
//
// Description:		Processes JSON responses from server.
//
// Input Arguments:
//		buffer	= const std::string & containing JSON string
//
// Output Arguments:
//		None
//
// Return Value:
//		bool, true for success, false otherwise
//
//==========================================================================
bool OAuth2Interface::HandleRefreshRequestResponse(const std::string &buffer, const bool &silent)
{
	cJSON *root = cJSON_Parse(buffer.c_str());
	if (!root)
	{
		if (!silent)
			std::cerr << "Failed to parse returned string (HandleRefreshRequsetResponse())" << std::endl;
		return false;
	}

	std::string tokenType;
	if (!ReadJSON(root, "refresh_token", refreshToken))
	{
		if (!silent)
			std::cerr << "Failed to read refresh token field from server" << std::endl;
		cJSON_Delete(root);
		return false;
	}
	cJSON_Delete(root);

	return HandleAccessRequestResponse(buffer);
}

//==========================================================================
// Class:			OAuth2Interface
// Function:		HandleAccessRequestResponse
//
// Description:		Processes JSON responses from server.
//
// Input Arguments:
//		buffer	= const std::string & containing JSON string
//
// Output Arguments:
//		None
//
// Return Value:
//		bool, true for success, false otherwise
//
//==========================================================================
bool OAuth2Interface::HandleAccessRequestResponse(const std::string &buffer)
{
	cJSON *root = cJSON_Parse(buffer.c_str());
	if (!root)
	{
		std::cerr << "Failed to parse returned string (HandleAccessRequestResponse())" << std::endl;
		return false;
	}

	std::string tokenType;
	if (!ReadJSON(root, "access_token", accessToken) ||
		!ReadJSON(root, "token_type", tokenType) ||
		!ReadJSON(root, "expires_in", accessTokenValidTime))
	{
		std::cerr << "Failed to read all required fields from server" << std::endl;
		cJSON_Delete(root);
		return false;
	}

	if (tokenType.compare("Bearer") != 0)
	{
		std::cerr << "Expected token type 'Bearer', received '" << tokenType << "'" << std::endl;
		cJSON_Delete(root);
		return false;
	}

	accessTokenObtainedTime = time(NULL);

	cJSON_Delete(root);
	return true;
}

//==========================================================================
// Class:			OAuth2Interface
// Function:		GetAccessToken
//
// Description:		Returns a valid access token.  This method generates new
//					access tokens as necessary (as they expire).
//
// Input Arguments:
//		None
//
// Output Arguments:
//		None
//
// Return Value:
//		std::string containing access token (or empty string on error)
//
//==========================================================================
std::string OAuth2Interface::GetAccessToken()
{
	// TODO:  Better way to check if access token is valid?  It would be good to be able
	// to request a new one after an API response with a 401 error.
	time_t now = time(NULL);
	if (!accessToken.empty() && difftime(now, accessTokenObtainedTime) < accessTokenValidTime)
		return accessToken;

	std::cout << "Access token is invalid - requesting a new one" << std::endl;

	std::string readBuffer;
	if (!DoCURLPost(tokenURL, AssembleAccessRequestQueryString(), readBuffer) ||
		ResponseContainsError(readBuffer) ||
		!HandleAccessRequestResponse(readBuffer))
	{
		std::cerr << "Failed to obtain access token" << std::endl;
		return "";
	}

	std::cout << "Successfully obtained new access token" << std::endl;
	return accessToken;
}

//==========================================================================
// Class:			OAuth2Interface
// Function:		AssembleRefreshRequestQueryString
//
// Description:		Assembles the proper request query string for obtaining a
//					refresh token.
//
// Input Arguments:
//		state	= const std::string&, anti-forgery state key
//
// Output Arguments:
//		None
//
// Return Value:
//		std::string containing access token (or empty string on error)
//
//==========================================================================
std::string OAuth2Interface::AssembleRefreshRequestQueryString(const std::string& state) const
{
	assert(!clientID.empty() &&
		!scope.empty());

	// Required fields
	std::string queryString;
	queryString.append("client_id=" + clientID);
	queryString.append("&scope=" + scope);

	// Optional fields
	if (!loginHint.empty())
		queryString.append("&login_hint=" + loginHint);

	if (!responseType.empty())
		queryString.append("&response_type=" + responseType);

	if (!redirectURI.empty())
		queryString.append("&redirect_uri=" + redirectURI);

	if (!state.empty())
		queryString.append("&state=" + state);

	return queryString;
}

//==========================================================================
// Class:			OAuth2Interface
// Function:		AssembleAccessRequestQueryString
//
// Description:		Assembles the proper request query string for obtaining an access
//					token.
//
// Input Arguments:
//		code	= const std::string&
//
// Output Arguments:
//		None
//
// Return Value:
//		std::string containing access token (or empty string on error)
//
//==========================================================================
std::string OAuth2Interface::AssembleAccessRequestQueryString(const std::string &code) const
{
	assert((!refreshToken.empty() || !code.empty()) &&
		!clientID.empty() &&
		!clientSecret.empty()/* &&
		!grantType.empty()*/);

	// Required fields
	std::string queryString;
	queryString.append("client_id=" + clientID);
	queryString.append("&client_secret=" + clientSecret);

	if (code.empty())
	{
		queryString.append("&refresh_token=" + refreshToken);
		queryString.append("&grant_type=refresh_token");
	}
	else
	{
		queryString.append("&code=" + code);
		queryString.append("&grant_type=" + grantType);
		if (!redirectURI.empty())
			queryString.append("&redirect_uri=" + redirectURI);
	}

	return queryString;
}

//==========================================================================
// Class:			OAuth2Interface
// Function:		RedirectURIIsLocal
//
// Description:		Checks to see if the URI indicates we should be listening
//					on a local port.
//
// Input Arguments:
//		None
//
// Output Arguments:
//		None
//
// Return Value:
//		bool
//
//==========================================================================
bool OAuth2Interface::RedirectURIIsLocal() const
{
	assert(!redirectURI.empty());
	const std::string localURL("http://localhost");

	return redirectURI.substr(0, localURL.length()).compare(localURL) == 0;
}

//==========================================================================
// Class:			OAuth2Interface
// Function:		StripPortFromLocalRedirectURI
//
// Description:		Parses the redirect URI string to obtain the local port number.
//
// Input Arguments:
//		None
//
// Output Arguments:
//		None
//
// Return Value:
//		int, contains port number or zero for error
//
//==========================================================================
int OAuth2Interface::StripPortFromLocalRedirectURI() const
{
	assert(RedirectURIIsLocal());

	unsigned int colon = redirectURI.find(':');
	if (colon == std::string::npos)
		return 0;

	std::stringstream s(redirectURI.substr(colon + 1));

	int port;
	s >> port;
	return port;
}

//==========================================================================
// Class:			OAuth2Interface
// Function:		GenerateSecurityStateKey
//
// Description:		Generates a random string of characters to use as a
//					security state key.
//
// Input Arguments:
//		None
//
// Output Arguments:
//		None
//
// Return Value:
//		std::string
//
//==========================================================================
std::string OAuth2Interface::GenerateSecurityStateKey() const
{
	std::string stateKey;
	while (stateKey.length() < 30)
		stateKey.append(Base36Encode((LongLong)rand()
			* (LongLong)rand() * (LongLong)rand() * (LongLong)rand()));

	return stateKey;
}

//==========================================================================
// Class:			OAuth2Interface
// Function:		Base36Encode
//
// Description:		Encodes the specified value in base36.
//
// Input Arguments:
//		value	= const LongLong&
//
// Output Arguments:
//		None
//
// Return Value:
//		std::string
//
//==========================================================================
std::string OAuth2Interface::Base36Encode(const LongLong &value)
{
	const unsigned int maxDigits(35);
	const char* charset = "abcdefghijklmnopqrstuvwxyz0123456789";
	std::string buf;
	buf.reserve(maxDigits);

	LongLong v(value);

	do
	{
		buf += charset[std::abs(v % 36)];
		v /= 36;
	} while (v);

	std::reverse(buf.begin(), buf.end());

	return buf;
}
