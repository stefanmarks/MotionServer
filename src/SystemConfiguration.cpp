#include "SystemConfiguration.h"

#include <string>
#include <algorithm>


/******************************************************************************
 * SystemConfiguration::sParameter class
 */

SystemConfiguration::sParameter::sParameter(
	const std::string& name, const std::string& parameter, const std::string& description) :
	name(name), parameter(parameter), description(description) 
{
	// prepare lowercase parameter
	std::transform(name.begin(), name.end(), std::back_inserter(nameLower), ::tolower);
}


/******************************************************************************
 * SystemConfiguration class
 */

SystemConfiguration::SystemConfiguration(const std::string& systemName) :
	systemName(systemName),
	parameters() 
{
	// nothing else to do
}


const std::string& SystemConfiguration::getSystemName() const
{ 
	return systemName; 
}


const std::vector<SystemConfiguration::sParameter>& SystemConfiguration::getParameters() const
{
	return parameters;
}


void SystemConfiguration::addOption(
	const std::string& name,
	const std::string& description)
{
	addParameter(name, "", description);
}


void SystemConfiguration::addParameter(
	const std::string& name,
	const std::string& parameter,
	const std::string& description)
{
	parameters.push_back(sParameter(name, parameter, description));
}


bool SystemConfiguration::processOption(const std::string& name)
{
	// convert input parameter name to lowercase
	std::string nameLower;
	std::transform(name.begin(), name.end(), std::back_inserter(nameLower), ::tolower);

	bool success = false;
	for (std::vector<sParameter>::const_iterator i = parameters.cbegin(); i != parameters.cend() && !success; i++)
	{
		if (((*i).nameLower == nameLower) && (*i).parameter.empty())
		{
			success |= handleParameter(i - parameters.cbegin(), "");
		}
	}
	return success;
}


bool SystemConfiguration::processParameter(const std::string& name, const std::string& value)
{
	// convert input parameter name to lowercase
	std::string nameLower;
	std::transform(name.begin(), name.end(), std::back_inserter(nameLower), ::tolower);

	bool success = false;
	for (std::vector<sParameter>::const_iterator i = parameters.cbegin(); i != parameters.cend() && !success; i++)
	{
		if (((*i).nameLower == nameLower) && !(*i).parameter.empty())
		{
			success |= handleParameter(i - parameters.cbegin(), value);
		}
	}
	return success;
}

