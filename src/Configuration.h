/**
 * Base class for parameterisation and configuration of a system through command line parameters.
 */

#pragma once

#include <string>
#include <sstream>
#include <vector>

/**
 * @brief Class for managing configurations of program components/systems through command line arguments.
 */
class Configuration
{
public:

	/**
	 * @brief Structre for a command line argument, its optional parameter, and its description.
	 */
	class Argument
	{
	public:
		
		/**
		 * @brief Constructs a command line argument with its name, parameter, and description.
		 *
		 * @param _name         the name of the argument
		 * @param _parameter    the parameter (optional, use "" when there is no parameter)
		 * @param _description  description of the argument
		 */
		Argument(const std::string& _name, const std::string& _parameter, const std::string& _description);

		/**
		 * @brief Gets the name of the argument.
		 *
		 * @return the name of the argument
		 */
		const std::string& getName() const { return name; }

		/**
		 * @brief Gets the parameter of the argument.
		 *
		 * @return the parameter of the argument
		 */
		const std::string& getParameter() const { return parameter; }

		/**
		 * @brief Gets the description of the argument.
		 *
		 * @return the description of the argument
		 */
		const std::string& getDescription() const { return description; }

	protected:

		friend class Configuration;

		std::string name;        ///< name of the argument
		std::string nameLower;   ///< lowercase name of the argument
		std::string parameter;   ///< parameter description for the argument
		std::string description; ///< description of the argument

	};


	/**
	 * @brief Creates a system confguration instance.
	 *
	 * @param _systemName  the name of the system to configure
	 */
	Configuration(const std::string& _systemName);


	/**
	 * @brief Gets the name of the system to configure.
	 *
	 * @return  the name of the system to configure
	 */
	const std::string& getSystemName() const;


	/**
	 * @brief Gets the list of command line arguments.
	 *
	 * @return  the list of command line arguments
	 */
	const std::vector<Argument>& getArguments() const;
	

	/**
	 * @brief Processes a command-line option (an argument without parameter, e.g., "-help").
	 *
	 * @param _name  the name of the option
	 *
	 * @return /c true if the option was processed, /c false if not
	 */
	bool processOption(const std::string& _name);


	/**
	 * @brief Processes a commad line parameter, e.g., "-address 127.0.0.1".
	 *
	 * @param _name   the name of the parameter
	 * @param _value  the value for the parameter
	 *
	 * @return \c true when the parameter was processed, <code>false</code> when not
	 */
	bool processParameter(const std::string& _name, const std::string& _value);

protected:

	/**
	 * @brief Adds an option to the list.
	 *
	 * @param _name         the name of the option
	 * @param _description  a description of the function of the option or parameter
	 */
	void addOption(const std::string& _name, const std::string& _description);
	
	
	/**
	 * @brief Adds a parameter to the list.
	 *
	 * @param _name         the name of the parameter
	 * @param _parameter    a description of the value for the parameter, e.g., "<address>"
	 * @param _description  a description of the function of the option or parameter
	 */
	void addParameter(const std::string& _name, const std::string& _parameter, const std::string& _description);


	/**
	 * @brief Adds a parameter to the list.
	 *
	 * @param _name          the name of the parameter
	 * @param _parameter     a description of the value for the parameter, e.g., "<address>"
	 * @param _description   a description of the function of the option or parameter
	 * @param _defaultValue  the default value to show in the description text
	 */
	template<typename T> void addParameter(const std::string& _name, const std::string& _parameter, const std::string& _description, const T& _defaultValue)
	{
		std::stringstream strm;
		strm << _description << " (default: " << _defaultValue << ")";
		arguments.push_back(Argument(_name, _parameter, strm.str()));
	}


	/**
	 * @brief Actually handle a command line argument.
	 *
	 * @param _idx    the index of the argument
	 * @param _value  the value for the parameter
	 *
	 * @return \c true if the argument was processed, \c false if not
	 */
	virtual bool handleArgument(unsigned int _idx, const std::string& _value) = 0;


protected:

	std::string systemName; ///< name of the system that is configured
	std::vector<Argument> arguments; ///< list of the command line arguments for the system
};
