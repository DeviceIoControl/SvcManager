// SvcManager.cpp : This file contains the 'main' function. Program execution begins and ends there.
#pragma once

#include <Windows.h>
#include <algorithm>
#include <iostream>
#include <vector>
#include <array>
#include <memory>

class ServiceHandle
{
public:
	ServiceHandle() noexcept
	{
		this->m_DispName = "N/A";
		this->m_SvcHandle = (SC_HANDLE)INVALID_HANDLE_VALUE;
	}

	ServiceHandle(SC_HANDLE hSvcHandle, std::string svcDispName)
	{
		this->m_SvcHandle = hSvcHandle;
		this->m_DispName = std::move(svcDispName);
	}

	ServiceHandle(const ServiceHandle& svcHandle) = delete;

	ServiceHandle(ServiceHandle&& svcHandle) noexcept
		: m_SvcHandle((SC_HANDLE)INVALID_HANDLE_VALUE)
	{
		std::swap(svcHandle.m_SvcHandle, this->m_SvcHandle);
		std::swap(svcHandle.m_DispName, this->m_DispName);
	}

	//Start the service.
	bool Start(const std::vector<std::string>& args) const
	{
		//I can't think of a safer way to do this...
		std::unique_ptr<const char*> lpArgs(new const char* [args.size()]);
		const char** lpszArgs = lpArgs.get();

		for (int i = 0; i < args.size(); ++i)
		{
			lpszArgs[i] = args[i].c_str();
		}

		return StartServiceA(this->m_SvcHandle, args.size(), lpArgs.get());
	}

	inline const std::string& Name() const { return this->m_DispName; }

	std::unique_ptr<QUERY_SERVICE_CONFIGA> QueryConfig() const
	{
		DWORD dwBytesRequired = NULL;
		std::unique_ptr<QUERY_SERVICE_CONFIGA> pSvcConfig = nullptr;

		if (!QueryServiceConfigA(this->m_SvcHandle, nullptr, 0, &dwBytesRequired)) 
		{
			auto ptr = reinterpret_cast<QUERY_SERVICE_CONFIGA*>(std::calloc(1, dwBytesRequired));
			pSvcConfig = std::unique_ptr<QUERY_SERVICE_CONFIGA>(ptr);
			
			if (!QueryServiceConfigA(this->m_SvcHandle, pSvcConfig.get(), dwBytesRequired, &dwBytesRequired)) 
			{
				std::cout << "Cannot query service configuration...\n";
				
				// After failing, we swap the unique_ptr into this scope for deletion and return a std::unique_ptr(nullptr)...
				std::unique_ptr<QUERY_SERVICE_CONFIGA> releasePtr( std::move(pSvcConfig) );
			}
		}

		// weird flex, but OK.
		return std::move(pSvcConfig);
	}

	//Check if the service handle is valid.
	inline bool Valid() const
	{
		return (this->m_SvcHandle != static_cast<SC_HANDLE>(INVALID_HANDLE_VALUE));
	}

	inline operator SC_HANDLE() const { return this->m_SvcHandle; }

	//Stop the service.
	inline bool Stop() const
	{
		SERVICE_STATUS svcStatus = { 0 };

		//Send a "SERVICE_CONTROL_STOP" message the service.
		BOOL success = ControlService(this->m_SvcHandle, SERVICE_CONTROL_STOP, &svcStatus);
		 
		//Check if the message is supported otherwise, we just return false.
		return (svcStatus.dwControlsAccepted & SERVICE_ACCEPT_STOP) ? success : false;
	}

	ServiceHandle& operator=(const ServiceHandle& rhs) = delete;

	inline ServiceHandle& operator=(ServiceHandle&& rhs) noexcept
	{
		std::swap(this->m_SvcHandle, rhs.m_SvcHandle);
		std::swap(this->m_DispName, rhs.m_DispName);
		return *this;
	}

	//NOTE: Only closes the service handle. This does NOT delete the service from the system.
	inline ~ServiceHandle()
	{
		CloseServiceHandle(this->m_SvcHandle);
	}

private:
	SC_HANDLE m_SvcHandle;
	std::string m_DispName;
};

//It will cause a declaration clash if I don't do this.
#undef OpenService
#undef CreateService

//Service access constants.
enum class SVC_ACCESS : uint32_t
{
	STOP = SERVICE_STOP,
	START = SERVICE_START,
	SUSPEND = SERVICE_PAUSE_CONTINUE,
	INTERROGATE = SERVICE_INTERROGATE,
	QUERY_CONFIG = SERVICE_QUERY_CONFIG,
	QUERY_STATUS = SERVICE_QUERY_STATUS,
	ENUMERATE_DEPENDENTS = SERVICE_ENUMERATE_DEPENDENTS,
	USER_DEFINED_CONTROL = SERVICE_USER_DEFINED_CONTROL,
	ALL_ACCESS = 0x000F0000 | SERVICE_STOP |
							SERVICE_START | 
							SERVICE_PAUSE_CONTINUE | 
							SERVICE_INTERROGATE | 
							SERVICE_QUERY_CONFIG | 
							SERVICE_QUERY_STATUS | 
							SERVICE_ENUMERATE_DEPENDENTS |
							SERVICE_USER_DEFINED_CONTROL
};

//Windows Service Start Types.
enum class SVC_START_TYPE : uint32_t
{
	BOOT = SERVICE_BOOT_START,
	AUTO = SERVICE_AUTO_START,
	MANUAL = SERVICE_DEMAND_START,
	SYSTEM = SERVICE_SYSTEM_START,
	DISABLED = SERVICE_DISABLED
};

//Windows Service Error Control params.
enum class SVC_ERROR_CTRL : uint32_t
{
	ERROR_IGNORE = SERVICE_ERROR_IGNORE,
	ERROR_NORMAL = SERVICE_ERROR_NORMAL,
	ERROR_SEVERE = SERVICE_ERROR_SEVERE,
	ERROR_CRITICAL = SERVICE_ERROR_CRITICAL
};

//Windows Service Types.
enum class SVC_TYPE : uint32_t
{
	KERNEL_DRIVER = SERVICE_KERNEL_DRIVER,
	USER_OWN_PROCESS = SERVICE_USER_OWN_PROCESS,
	FILE_SYS_DRIVER = SERVICE_FILE_SYSTEM_DRIVER,
	WIN32_OWN_PROCESS = SERVICE_WIN32_OWN_PROCESS,
	USER_SHARE_PROCESS = SERVICE_USER_SHARE_PROCESS,
	WIN32_SHARE_PROCESS = SERVICE_WIN32_SHARE_PROCESS
};

class ServiceManager
{
public:
	inline static bool Initialise(const std::string& machineName = "")
	{
		ServiceManager::m_SvcManager = OpenSCManagerA(machineName.c_str(), nullptr, SC_MANAGER_ALL_ACCESS);
		return (ServiceManager::m_SvcManager == INVALID_HANDLE_VALUE) ? false : true;
	}

	//Creates a user specified service.
	static ServiceHandle CreateService(const std::string& svcName, const std::string& svcDispName,
		SVC_TYPE ServiceType, SVC_START_TYPE StartType, SVC_ERROR_CTRL ErrorControl,
		const std::string& svcBinPath, const std::string& svcUserName = "",
		const std::string& svcUserPwd = "")
	{
		DWORD SvcTagId = 0;

		SC_HANDLE serviceHandle = CreateServiceA(ServiceManager::m_SvcManager, svcName.c_str(),
			svcDispName.c_str(), SERVICE_ALL_ACCESS, static_cast<DWORD>(ServiceType), static_cast<DWORD>(StartType),
			static_cast<DWORD>(ErrorControl), svcBinPath.c_str(), nullptr, &SvcTagId, nullptr, svcUserName.c_str(),
			svcUserPwd.c_str());

		return ServiceHandle(serviceHandle, svcDispName);
	}

	static ServiceHandle OpenService(const std::string& svcName, SVC_ACCESS desiredAccess)
	{
		SC_HANDLE svcHandle = OpenServiceA(ServiceManager::m_SvcManager,
			svcName.c_str(), static_cast<DWORD>(desiredAccess));

		std::array<char, 0x1000> dispNameChars = { 0 };
		DWORD dispNameCharArraySize = dispNameChars.size();
		
		if (GetServiceDisplayNameA(ServiceManager::m_SvcManager, svcName.c_str(),
			dispNameChars.data(), &dispNameCharArraySize)) 
		{
			return ServiceHandle(svcHandle, std::string(dispNameChars.data()));
		}
 
		return ServiceHandle(svcHandle, "N/A");
	}

	//Deletes the service.
	inline static bool DeleteService(const ServiceHandle& svcHandle)
	{
		return ::DeleteService(svcHandle);
	}

	//Shutdown the Service Manager.
	inline static bool Shutdown()
	{
		return CloseServiceHandle(ServiceManager::m_SvcManager);
	}

private:
	inline static SC_HANDLE m_SvcManager = (SC_HANDLE)INVALID_HANDLE_VALUE;
};

// Run program: Ctrl + F5 or Debug > Start Without Debugging menu
// Debug program: F5 or Debug > Start Debugging menu

// Tips for Getting Started: 
//   1. Use the Solution Explorer window to add/manage files
//   2. Use the Team Explorer window to connect to source control
//   3. Use the Output window to see build output and other messages
//   4. Use the Error List window to view errors
//   5. Go to Project > Add New Item to create new code files, or Project > Add Existing Item to add existing code files to the project
//   6. In the future, to open this project again, go to File > Open > Project and select the .sln file
