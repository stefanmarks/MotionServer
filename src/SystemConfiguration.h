/**
 * Base class for parameterisation and configuration of a system through command line parameters.
 */

#pragma once

#include <string>
#include <vector>


class SystemConfiguration
{
public:

	/**
	 * Structre for a command line parameter, its argument, and its description.
	 */
	struct sParameter
	{
		std::string name;
		std::string nameLower;
		std::string parameter;
		std::string description;

		sParameter(const std::string& name, const std::string& parameter, const std::string& description);
	};


	/**
	 * Creates a system confguration instance.
	 *
	 * @param name  the name of the system to configure
	 */
	SystemConfiguration(const std::string& name);


	/**
	 * Gets the name of the system to configure.
	 *
	 * @return  the name of the system to configure
	 */
	const std::string& getSystemName() const;


	/**
	 * Gets the list of parameters.
	 *
	 * @param name  the list of parameters
	 */
	const std::vector<sParameter>& getParameters() const;
	

	/**
	 * Processes a commadline option, e.g., "-help".
	 *
	 * @param name  the name of the option
	 *
	 * @return <code>true</code> when the option was processed, <code>false</code> when not
	 */
	bool processOption(const std::string& name);


	/**
	 * Processes a commadline parameter, e.g., "-address 127.0.0.1".
	 *
	 * @param name   the name of the parameter
	 * @param value  the value for the parameter
	 *
	 * @return <code>true</code> when the parameter was processed, <code>false</code> when not
	 */
	bool processParameter(const std::string& name, const std::string& value);

protected:

	/**
	 * Adds an option to the list.
	 *
	 * @param name         the name of the option
	 * @param description  a description of the function of the option or parameter
	 */
	void addOption(const std::string& name, const std::string& description);
	
	
	/**
	* Adds a parameter to the list.
	*
	* @param name         the name of the parameter
	* @param parameter    a description of the value for the parameter, e.g., "<address>"
	* @param description  a description of the function of the option or parameter
	*/
	void addParameter(const std::string& name, const std::string& parameter, const std::string& description);


	/**
	 * Actually handle an option or parameter.
	 *
	 * @param idx    the index of the parameter/option
	 * @param value  the value for the parameter
	 *
	 * @return <code>true</code> when the parameter was processed, <code>false</code> when not
	 */
	virtual bool handleParameter(int idx, const std::string& value) = 0;

protected:

	std::string             systemName;
	std::vector<sParameter> parameters;
};