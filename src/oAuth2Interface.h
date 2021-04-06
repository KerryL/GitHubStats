// File:  oAuth2Interface.h
// Date:  4/15/2013
// Auth:  K. Loux
// Desc:  Handles interface to a server using OAuth 2.0.  Implemented as a thread-safe
//        singleton.

#ifndef OAUTH2_INTERFACE_H_
#define OAUTH2_INTERFACE_H_

// Standard C++ headers
#include <chrono>

// email headers
#include "jsonInterface.h"

// Standard C++ headers
#include <string>
#include <iostream>

// cJSON (local) forward declarations
struct cJSON;

class OAuth2Interface : public JSONInterface
{
public:
	~OAuth2Interface();

	static OAuth2Interface& Get();
	static void Destroy();

	void SetLoggingTarget(std::ostream& newLog) { log = &newLog; }

	void SetAuthenticationURL(const std::string &authURLIn) { authURL = authURLIn; }
	void SetAuthenticationPollURL(const std::string &authPollURLIn) { authPollURL = authPollURLIn; }
	void SetTokenURL(const std::string &tokenURLIn) { tokenURL = tokenURLIn; }
	void SetResponseType(const std::string &responseTypeIn) { responseType = responseTypeIn; }
	void SetClientID(const std::string &clientIDIn) { clientID = clientIDIn; }
	void SetClientSecret(const std::string &clientSecretIn) { clientSecret = clientSecretIn; }
	void SetRedirectURI(const std::string &redirectURIIn) { redirectURI = redirectURIIn; }
	void SetScope(const std::string &scopeIn) { scope = scopeIn; }
	void SetLoginHint(const std::string &loginHintIn) { loginHint = loginHintIn; }
	void SetGrantType(const std::string &grantTypeIn) { grantType = grantTypeIn; }
	void SetPollGrantType(const std::string &pollGrantTypeIn) { pollGrantType = pollGrantTypeIn; }

	void SetRefreshToken(const std::string &refreshTokenIn = std::string());

	void SetSuccessMessage(const std::string& message) { successMessage = message; }

	std::string GetRefreshToken() const { return refreshToken; }
	std::string GetAccessToken();

	static std::string Base36Encode(const int64_t &value);

private:
	OAuth2Interface();

	std::ostream* log = &std::cout;

	std::string authURL;
	std::string authPollURL;
	std::string tokenURL;
	std::string responseType;
	std::string clientID;
	std::string clientSecret;
	std::string redirectURI;
	std::string scope;
	std::string loginHint;
	std::string grantType;
	std::string pollGrantType;

	std::string refreshToken;
	std::string accessToken;

	std::string successMessage = "API access successfulley authorized.";

	std::string RequestRefreshToken();

	std::string AssembleRefreshRequestQueryString(const std::string &state = std::string()) const;
	std::string AssembleAccessRequestQueryString(const std::string &code = std::string(), const bool& usePollGrantType = false) const;

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
	bool IsLimitedInput() const { return redirectURI.empty(); }
	unsigned short StripPortFromLocalRedirectURI() const;
	std::string StripAddressFromLocalRedirectURI() const;

	std::chrono::system_clock::time_point accessTokenValidUntilTime;

	static OAuth2Interface *singleton;
	static std::string ExtractAuthCodeFromGETRequest(const std::string& rawRequest);

	static std::string BuildHTTPSuccessResponse(const std::string& successMessage);

	struct AdditionalCurlData : public ModificationData
	{
		AdditionalCurlData(struct curl_slist*& headerList) : headerList(headerList) {}
		struct curl_slist*& headerList;
	};

	AdditionalCurlData additionalData;
	struct curl_slist* headerList = nullptr;
	static bool AddAdditionalCurlData(CURL* curl, const ModificationData* data);
};

#endif// OAUTH2_INTERFACE_H_