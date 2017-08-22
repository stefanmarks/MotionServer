#include "Configuration.h"

#include <string>
#include <algorithm>


/******************************************************************************
 * Configuration::Argument class
 */

Configuration::Argument::Argument(
	const std::string& _name, const std::string& _parameter, const std::string& _description) :
	name(_name), parameter(_parameter), description(_description)
{
	// prepare lowercase parameter
	std::transform(_name.begin(), _name.end(), std::back_inserter(nameLower), ::tolower);
}


/******************************************************************************
 * Configuration class
 */

Configuration::Configuration(const std::string& _systemName) :
	systemName(_systemName),
	arguments()
{
	// nothing else to do
}


const std::string& Configuration::getSystemName() const
{
	return systemName;
}


const std::vector<Configuration::Argument>& Configuration::getArguments() const
{
	return arguments;
}


void Configuration::addOption(
	const std::string& _name,
	const std::string& _description)
{
	addParameter(_name, "", _description);
}


void Configuration::addParameter(
	const std::string& _name,
	const std::string& _parameter,
	const std::string& _description)
{
	arguments.push_back(Argument(_name, _parameter, _description));
}


bool Configuration::processOption(const std::string& _name)
{
	// convert input parameter name to lowercase
	std::string nameLower;
	std::transform(_name.begin(), _name.end(), std::back_inserter(nameLower), ::tolower);

	bool success = false;
	for (auto paramIter = arguments.cbegin(); paramIter != arguments.cend() && !success; paramIter++)
	{
		// if this is an option (no parameter) and the name matches...
		if ((*paramIter).parameter.empty() && ((*paramIter).nameLower == nameLower))
		{
			// ...handle it
			success |= handleArgument((unsigned int)(paramIter - arguments.cbegin()), "");
		}
	}
	return success;
}


bool Configuration::processParameter(const std::string& _name, const std::string& _value)
{
	// convert input parameter name to lowercase
	std::string nameLower;
	std::transform(_name.begin(), _name.end(), std::back_inserter(nameLower), ::tolower);

	bool success = false;
	for (auto paramIter = arguments.cbegin(); paramIter != arguments.cend() && !success; paramIter++)
	{
		// if this is a parameter (not an option) and the name matches...
		if (!(*paramIter).parameter.empty() && ((*paramIter).nameLower == nameLower))
		{
			// ...handle it
			success |= handleArgument((unsigned int)(paramIter - arguments.cbegin()), _value);
		}
	}
	return success;
}
