// File:  oAuth2Interface.h
// Date:  4/15/2013
// Auth:  K. Loux
// Desc:  Handles interface to a server using OAuth 2.0.  Implemented as a thread-safe
//        singleton.

#ifndef OAUTH2_INTERFACE_H_
#define OAUTH2_INTERFACE_H_

// Standard C++ headers
#include <string>
#include <ctime>

// utilities headers
#include "jsonInterface.h"
#include "portable.h"

// cJSON (local) forward declarations
struct cJSON;

class OAuth2Interface : public JSONInterface
{
public:

	static OAuth2Interface& Get();
	static void Destroy();

	void SetAuthenticationURL(const std::string &authURLIn) { authURL = authURLIn; }
	void SetTokenURL(const std::string &tokenURLIn) { tokenURL = tokenURLIn; }
	void SetResponseType(const std::string &responseTypeIn) { responseType = responseTypeIn; }
	void SetClientID(const std::string &clientIDIn) { clientID = clientIDIn; }
	void SetClientSecret(const std::string &clientSecretIn) { clientSecret = clientSecretIn; }
	void SetRedirectURI(const std::string &redirectURIIn) { redirectURI = redirectURIIn; }
	void SetScope(const std::string &scopeIn) { scope = scopeIn; }
	void SetLoginHint(const std::string &loginHintIn) { loginHint = loginHintIn; }
	void SetGrantType(const std::string &grantTypeIn) { grantType = grantTypeIn; }
	
	void SetRefreshToken(const std::string &refreshTokenIn = "");

	std::string GetRefreshToken() const { return refreshToken; }
	std::string GetAccessToken();

	static std::string Base36Encode(const LongLong &value);

private:
	OAuth2Interface();
	~OAuth2Interface();

	std::string authURL, tokenURL;
	std::string responseType;
	std::string clientID;
	std::string clientSecret;
	std::string redirectURI;
	std::string scope;
	std::string loginHint;
	std::string grantType;

	std::string refreshToken;
	std::string accessToken;

	std::string RequestRefreshToken();

	std::string AssembleRefreshRequestQueryString(const std::string &state = "") const;
	std::string AssembleAccessRequestQueryString(const std::string &code = "") const;

	struct AuthorizationResponse
	{
		std::string deviceCode;
		double expiresIn;
		int interval;
	};

	bool HandleAuthorizationRequestResponse(const std::string &buffer,
		AuthorizationResponse &response);
	bool HandleRefreshRequestResponse(const std::string &buffer, const bool &silent = false);
	bool HandleAccessRequestResponse(const std::string &buffer);
	bool ResponseContainsError(const std::string &buffer);

	std::string GenerateSecurityStateKey() const;
	bool RedirectURIIsLocal() const;
	bool IsLimitedInput() { return redirectURI.empty(); };
	int StripPortFromLocalRedirectURI() const;

	// timing information to determine if we need to request a new code

	time_t accessTokenObtainedTime;
	double accessTokenValidTime;// [sec]

	static OAuth2Interface *singleton;
};

#endif// OAUTH2_INTERFACE_H_