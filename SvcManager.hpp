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

	ServiceHandle(SC_HANDLE hSvcHandle, const std::string& svcDispName)
	{
		this->m_SvcHandle = hSvcHandle;
		this->m_DispName = svcDispName;
	}

	ServiceHandle(ServiceHandle&& svcHandle) noexcept
		: m_SvcHandle((SC_HANDLE)INVALID_HANDLE_VALUE)
	{
		std::swap(svcHandle.m_SvcHandle, this->m_SvcHandle);
		std::swap(svcHandle.m_DispName, this->m_DispName);
	}

	// Start the service.
	bool Start(const std::vector<std::string>& args) const
	{
		// Data in a vector is guaranteed to be contigous.
		std::vector<const char*> lpArgs(args.cbegin(), args.cend());
		return StartServiceA(this->m_SvcHandle, args.size(), lpArgs.data());
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

		return std::move(pSvcConfig);
	}

	// Check if the service handle is valid.
	inline bool Valid() const
	{
		return (this->m_SvcHandle != static_cast<SC_HANDLE>(INVALID_HANDLE_VALUE)
			&& this->m_SvcHandle != NULL);
	}

	inline operator SC_HANDLE() const { return this->m_SvcHandle; }

	// Stop the service.
	inline bool Stop() const
	{
		SERVICE_STATUS svcStatus = { 0 };

		// Send a "SERVICE_CONTROL_STOP" message the service.
		const bool success = ControlService(this->m_SvcHandle, SERVICE_CONTROL_STOP, &svcStatus);
		 
		// Check if the message is supported otherwise, we just return false.
		return (svcStatus.dwControlsAccepted & SERVICE_ACCEPT_STOP) ? success : false;
	}

	// Non-copyable.
	ServiceHandle(const ServiceHandle& svcHandle) = delete;
	ServiceHandle& operator=(const ServiceHandle& rhs) = delete;

	inline ServiceHandle& operator=(ServiceHandle&& rhs) noexcept
	{
		std::swap(this->m_SvcHandle, rhs.m_SvcHandle);
		std::swap(this->m_DispName, rhs.m_DispName);
		return *this;
	}

	// NOTE: Only closes the service handle. This does NOT delete the service from the system.
	inline ~ServiceHandle()
	{
		if(this->m_SvcHandle != INVALID_HANDLE_VALUE)
		{
			CloseServiceHandle(this->m_SvcHandle);
		}
	}

private:
	SC_HANDLE m_SvcHandle;
	std::string m_DispName;
};

// Avoid declaration clash.
#undef OpenService
#undef CreateService

// Service access constants.
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

// Windows Service Start Types.
enum class SVC_START_TYPE : uint32_t
{
	BOOT = SERVICE_BOOT_START,
	AUTO = SERVICE_AUTO_START,
	MANUAL = SERVICE_DEMAND_START,
	SYSTEM = SERVICE_SYSTEM_START,
	DISABLED = SERVICE_DISABLED
};

// Windows Service Error Control params.
enum class SVC_ERROR_CTRL : uint32_t
{
	ERROR_IGNORE = SERVICE_ERROR_IGNORE,
	ERROR_NORMAL = SERVICE_ERROR_NORMAL,
	ERROR_SEVERE = SERVICE_ERROR_SEVERE,
	ERROR_CRITICAL = SERVICE_ERROR_CRITICAL
};

// Windows Service Types.
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

	// Creates a user specified service.
	static ServiceHandle CreateService(const std::string& svcName, const std::string& svcDispName,
		SVC_TYPE ServiceType, SVC_START_TYPE StartType, SVC_ERROR_CTRL ErrorControl,
		const std::string& svcBinPath, const std::string& svcUserName = "",
		const std::string& svcUserPwd = "")
	{
		const char* lpszSvcUserName = (svcUserName.empty()) ? nullptr : svcUserName.c_str();
		const char* lpszUserPwd = (svcUserPwd.empty()) ? nullptr : svcUserPwd.c_str();

		SC_HANDLE serviceHandle = CreateServiceA(ServiceManager::m_SvcManager, svcName.c_str(),
			svcDispName.c_str(), SERVICE_ALL_ACCESS, static_cast<DWORD>(ServiceType), static_cast<DWORD>(StartType),
			static_cast<DWORD>(ErrorControl), svcBinPath.c_str(), nullptr, nullptr, nullptr, lpszSvcUserName,
			lpszUserPwd);

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

	// Deletes the service.
	inline static bool DeleteService(const ServiceHandle& svcHandle)
	{
		return ::DeleteService(svcHandle);
	}

	// Shutdown the Service Manager.
	inline static bool Shutdown()
	{
		return CloseServiceHandle(ServiceManager::m_SvcManager);
	}

private:
	inline static SC_HANDLE m_SvcManager = static_cast<SC_HANDLE>(INVALID_HANDLE_VALUE);
};
