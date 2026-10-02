param(
	[Parameter(Mandatory, Position = 0)]
	[string]$Message
)

Invoke-WebRequest https://wtxt.azurewebsites.net/value -Method Post -ContentType 'text/plain; charset=utf-8' -Body $Message

# Examples:
#
# 	.\wtxt.ps1 "Hello, World!"
# 	.\wtxt.ps1 "<red/><large/>Hello !!!"
# 	.\wtxt.ps1 "<green/><medium/>Hello !!!"
# 	.\wtxt.ps1 "<blue/><small/>Hello !!!"
# 	.\wtxt.ps1 "<cyan/><large/>Hello !!!"
# 	.\wtxt.ps1 "<magenta/><large/>Hello !!!"
# 	.\wtxt.ps1 "<yellow/><large/>Hello !!!"
# 	.\wtxt.ps1 "<white/><large/>Hello !!!"
