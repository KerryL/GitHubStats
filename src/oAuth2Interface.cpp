// File:  oAuth2Interface.cpp
// Date:  4/15/2013
// Auth:  K. Loux
// Desc:  Handles interface to a server using OAuth 2.0.

// Standard C++ headers
#include <cstdlib>
#include <cassert>
#include <sstream>
#include <ctime>
#include <algorithm>

// Standard C headers
#include <string.h>
#include <cassert>

// cURL headers
#include <curl/curl.h>

// utilities headers
#include "cppSocket.h"
#include "cJSON.h"

// OS headers
#ifdef _WIN32
#include <Windows.h>
#include <shellapi.h>
#else
#include <unistd.h>
#endif

// Local headers
#include "oAuth2Interface.h"

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
OAuth2Interface *OAuth2Interface::singleton = nullptr;

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
OAuth2Interface::OAuth2Interface() : additionalData(headerList)
{
	verbose = false;
}

OAuth2Interface::~OAuth2Interface()
{
	if (headerList)
		curl_slist_free_all(headerList);
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

	singleton = nullptr;
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
		refreshToken = RequestRefreshToken();// TODO:  Check for errors (returned empty UString::?)
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
//		std::string containing refresh token, or emtpy std::string on error
//
//==========================================================================
std::string OAuth2Interface::RequestRefreshToken()
{
	assert(!authURL.empty() && !tokenURL.empty());

	if (IsLimitedInput())
	{
		std::string rawReadBuffer;
		if (!DoCURLPost(authURL, AssembleRefreshRequestQueryString(), rawReadBuffer, &AddAdditionalCurlData, &additionalData))
			return std::string();

		std::string readBuffer(rawReadBuffer);

		if (ResponseContainsError(readBuffer))
			return std::string();

		AuthorizationResponse authResponse;
		if (!HandleAuthorizationRequestResponse(readBuffer, authResponse))
			return std::string();

		std::string queryString = AssembleAccessRequestQueryString(authResponse.deviceCode, !pollGrantType.empty());

		time_t startTime = time(nullptr);
		time_t now = startTime;
		while (!HandleRefreshRequestResponse(readBuffer, true))
		{
#ifdef _WIN32
			Sleep(1000 * authResponse.interval);
#else
			sleep(authResponse.interval);
#endif
			now = time(nullptr);
			if (difftime(now, startTime) > authResponse.expiresIn)
			{
				*log << "Request timed out - restart application to start again" << std::endl;
				return std::string();
			}

			if (!DoCURLPost(authPollURL, queryString, rawReadBuffer, &AddAdditionalCurlData, &additionalData))
				return std::string();
			readBuffer = rawReadBuffer;

			if (ResponseContainsError(readBuffer))
				return std::string();
		}
	}
	else
	{
		assert(!responseType.empty());

		std::string stateKey;// = GenerateSecurityStateKey();// TODO:  Need to finish implementation with state key

		const std::string assembledAuthURL(authURL + '?' + AssembleRefreshRequestQueryString(stateKey));
		CPPSocket webSocket(CPPSocket::SocketType::SocketTCPServer, *log);
		if (RedirectURIIsLocal())
		{
			if (!webSocket.Create(StripPortFromLocalRedirectURI(), StripAddressFromLocalRedirectURI().c_str()))
				return std::string();
#ifdef _WIN32
			ShellExecuteA(nullptr, "open", assembledAuthURL.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
#else
			system(UString::ToNarrowString(std::string(_T("xdg-open '")) + assembledAuthURL + std::string(_T("'"))).c_str());
#endif
		}
		else// (for example, with redirect URI set to "oob")
		{
			// The benefit of doing it the way we're doing it now, though, is
			// that the browser used to authenticate does not need to be on the
			// same machine that is running this application.
			std::cout << "Enter this address in your browser:" << std::endl << assembledAuthURL << std::endl;
		}

		std::string authorizationCode;
		if (RedirectURIIsLocal())
		{
			if (!webSocket.WaitForClientData(60000))
			{
				*log << "No response... aborting" << std::endl;
				return std::string();
			}

			std::string message;
			{
				std::lock_guard<std::mutex> lock(webSocket.GetMutex());
				const auto messageSize(webSocket.Receive());
				if (messageSize <= 0)
					return std::string();
				message.assign(reinterpret_cast<char*>(webSocket.GetLastMessage()), messageSize);
			}

			if (message.empty())
				return std::string();

			authorizationCode = ExtractAuthCodeFromGETRequest(message);
			const auto successResponse(BuildHTTPSuccessResponse(successMessage));
			assert(successResponse.length() < std::numeric_limits<unsigned int>::max());
			if (!webSocket.TCPSend(reinterpret_cast<const CPPSocket::DataType*>(successResponse.c_str()), static_cast<int>(successResponse.length())))
				*log << "Warning:  Authorization code response failed to send" << std::endl;
		}
		else
		{
			std::cout << "Enter verification code:" << std::endl;
			std::cin >> authorizationCode;
		}

		std::string readBuffer;
		if (!DoCURLPost(tokenURL, AssembleAccessRequestQueryString(authorizationCode), readBuffer, &AddAdditionalCurlData, &additionalData) ||
			ResponseContainsError(readBuffer) ||
			!HandleRefreshRequestResponse(readBuffer))
		{
			*log << "Failed to obtain refresh token" << std::endl;
			return std::string();
		}
	}

	*log << "Successfully obtained refresh token" << std::endl;
	return refreshToken;
}

std::string OAuth2Interface::ExtractAuthCodeFromGETRequest(const std::string& rawRequest)
{
	std::string request(rawRequest);
	const std::string startKey("?code=");
	const auto start(request.find(startKey));
	if (start == std::string::npos)
		return std::string();

	const std::string endKey(" HTTP/1.1");
	const auto end(request.find(endKey, start));
	if (end == std::string::npos)
		return std::string();
	return request.substr(start + startKey.length(), end - start - startKey.length());
}

std::string OAuth2Interface::BuildHTTPSuccessResponse(const std::string& successMessage)
{
	std::string body("<html><body><h1>Success!</h1><p>" + successMessage + "</p></body></html>");

	std::ostringstream headerStream;
	headerStream << "HTTP/1.1 200 OK\n"
		<< "Date: Sun, 18 Oct 2009 08:56:53 GMT\n"
		<< "Server: eBirdDataProcessor\n"
		<< "Last-Modified: Sat, 20 Nov 2004 07:16:26 GMT\n"
		<< "Accept-Ranges: bytes\n"
		<< "Content-Length: " << body.length() << '\n'
		<< "Connection: close\n"
		<< "Content-Type: text/html\n\n";

	return headerStream.str() + body;
}

//==========================================================================
// Class:			OAuth2Interface
// Function:		ResponseContainsError
//
// Description:		Checks JSON array to see if there is an error entry.
//					"Authorization pending" errors are not considered errors.
//
// Input Arguments:
//		buffer		= const std::string & containing JSON UString::
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
	cJSON *root(cJSON_Parse(buffer.c_str()));
	if (!root)
	{
		*log << "Failed to parse returned string (ResponseContainsError())" << std::endl;
		if (verbose)
			std::cerr << buffer << '\n';
		return true;
	}

	std::string error;
	if (ReadJSON(root, "error", error))
	{
		if (error != "authorization_pending")
		{
			std::ostringstream ss;
			ss << "Recieved error from OAuth server:  " << error;
			std::string description;
			if (ReadJSON(root, "error_description", description))
				ss << " - " << description;
			*log << ss.str() << std::endl;
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
//		buffer		= const std::string & containing JSON UString::
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

	cJSON *root(cJSON_Parse(buffer.c_str()));
	if (!root)
	{
		*log << "Failed to parse returned string (HandleAuthorizationRequestResponse())" << std::endl;
		return false;
	}

	std::string userCode, verificationURL;
	if (!ReadJSON(root, "verification_url", verificationURL))
	{
		if (!ReadJSON(root, "verification_uri", verificationURL))
		{
			cJSON_Delete(root);
			return false;
		}
	}

	// TODO:  Check state key?
	if (!ReadJSON(root, "device_code", response.deviceCode) ||
		!ReadJSON(root, "user_code", userCode) ||
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
//		buffer	= const std::string& containing JSON
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
			*log << "Failed to parse returned string (HandleRefreshRequsetResponse())" << std::endl;
		return false;
	}

	if (!ReadJSON(root, "refresh_token", refreshToken))
	{
		if (!ReadJSON(root, "access_token", refreshToken))
		{
			if (!silent)
				*log << "Failed to read refresh token field from server" << std::endl;
			cJSON_Delete(root);
			return false;
		}
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
//		buffer	= const std::string & containing JSON UString::
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
		*log << "Failed to parse returned string (HandleAccessRequestResponse())" << std::endl;
		return false;
	}

	std::string tokenType;
	std::string scopes;
	unsigned int tokenValidDuration;// [sec]
	if (!ReadJSON(root, "access_token", accessToken) ||
		!ReadJSON(root, "token_type", tokenType) ||
		!ReadJSON(root, "scope", scopes))
	{
		*log << "Failed to read all required fields from server" << std::endl;
		cJSON_Delete(root);
		return false;
	}

	*log << "Received token for the following scopes:  " << scopes << std::endl;

	if (tokenType != "Bearer" && tokenType != "bearer")
	{
		*log << "Expected token type 'Bearer', received '" << tokenType << "'" << std::endl;
		cJSON_Delete(root);
		return false;
	}

	if (ReadJSON(root, "expires_in", tokenValidDuration))
		accessTokenValidUntilTime = std::chrono::system_clock::now() + std::chrono::seconds(tokenValidDuration);

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
//		std::string containing access token (or empty std::string on error)
//
//==========================================================================
std::string OAuth2Interface::GetAccessToken()
{
	// TODO:  Better way to check if access token is valid?  It would be good to be able
	// to request a new one after an API response with a 401 error.
	if (!accessToken.empty() && std::chrono::system_clock::now() < accessTokenValidUntilTime)
		return accessToken;

	*log << "Access token is invalid - requesting a new one" << std::endl;

	std::string readBuffer;
	if (!DoCURLPost(tokenURL, AssembleAccessRequestQueryString(), readBuffer, &AddAdditionalCurlData, &additionalData) ||
		ResponseContainsError(readBuffer) ||
		!HandleAccessRequestResponse(readBuffer))
	{
		*log << "Failed to obtain access token" << std::endl;
		return std::string();
	}

	*log << "Successfully obtained new access token" << std::endl;
	return accessToken;
}

//==========================================================================
// Class:			OAuth2Interface
// Function:		AssembleRefreshRequestQueryString
//
// Description:		Assembles the proper request query std::string for obtaining a
//					refresh token.
//
// Input Arguments:
//		state	= const std::string&, anti-forgery state key
//
// Output Arguments:
//		None
//
// Return Value:
//		std::string containing access token (or empty std::string on error)
//
//==========================================================================
std::string OAuth2Interface::AssembleRefreshRequestQueryString(const std::string& state) const
{
	assert(!clientID.empty());

	// Required fields
	std::string queryString;
	queryString.append("client_id=" + clientID);

	if (!scope.empty())
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
// Description:		Assembles the proper request query std::string for obtaining an access
//					token.
//
// Input Arguments:
//		code				= const std::string&
//		usePollGrantType	= const bool&
//
// Output Arguments:
//		None
//
// Return Value:
//		String containing access token (or empty std::string on error)
//
//==========================================================================
std::string OAuth2Interface::AssembleAccessRequestQueryString(const std::string &code, const bool& usePollGrantType) const
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
		if (IsLimitedInput())
			queryString.append("&device_code=" + code);
		else
			queryString.append("&code=" + code);

		if (usePollGrantType)
		{
			assert(!pollGrantType.empty());
			queryString.append("&grant_type=" + pollGrantType);
		}
		else
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
	const std::string localURL1("http://localhost");
	const std::string localURL2("http://127.0.0.1");

	return redirectURI.substr(0, localURL1.length()).compare(localURL1) == 0 ||
		redirectURI.substr(0, localURL2.length()).compare(localURL2) == 0;
}

//==========================================================================
// Class:			OAuth2Interface
// Function:		StripPortFromLocalRedirectURI
//
// Description:		Parses the redirect URI std::string to obtain the local port number.
//
// Input Arguments:
//		None
//
// Output Arguments:
//		None
//
// Return Value:
//		unsigned short, contains port number or zero for error
//
//==========================================================================
unsigned short OAuth2Interface::StripPortFromLocalRedirectURI() const
{
	assert(RedirectURIIsLocal());

	const size_t colon(redirectURI.find_last_of(':'));
	if (colon == std::string::npos)
		return 0;

	std::istringstream s(redirectURI.substr(colon + 1));

	unsigned short port;
	s >> port;
	return port;
}


std::string OAuth2Interface::StripAddressFromLocalRedirectURI() const
{
	assert(RedirectURIIsLocal());

	const size_t colon(redirectURI.find_last_of(':'));
	if (colon == std::string::npos)
		return redirectURI;
	return redirectURI.substr(0, colon);
}

//==========================================================================
// Class:			OAuth2Interface
// Function:		GenerateSecurityStateKey
//
// Description:		Generates a random std::string of characters to use as a
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
		stateKey.append(Base36Encode((int64_t)rand()
			* (int64_t)rand() * (int64_t)rand() * (int64_t)rand()));

	return stateKey;
}

//==========================================================================
// Class:			OAuth2Interface
// Function:		Base36Encode
//
// Description:		Encodes the specified value in base36.
//
// Input Arguments:
//		value	= const int64_t&
//
// Output Arguments:
//		None
//
// Return Value:
//		std::string
//
//==========================================================================
std::string OAuth2Interface::Base36Encode(const int64_t &value)
{
	const unsigned int maxDigits(35);
	const char* charset = "abcdefghijklmnopqrstuvwxyz0123456789";
	std::string buf;
	buf.reserve(maxDigits);

	int64_t v(value);

	do
	{
		buf += charset[std::abs(v % 36)];
		v /= 36;
	} while (v);

	std::reverse(buf.begin(), buf.end());

	return buf;
}

bool OAuth2Interface::AddAdditionalCurlData(CURL* curl, const ModificationData* data)
{
	auto headerList(dynamic_cast<const AdditionalCurlData*>(data)->headerList);
	headerList = curl_slist_append(headerList, "Accept: application/json");
	if (!headerList)
	{
		std::cerr << "Failed to create accept header\n";
		return false;
	}

	if (curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headerList))
	{
		std::cerr << "Failed to add header list\n";
		return false;
	}
	
	return true;
}
