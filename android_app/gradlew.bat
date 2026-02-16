@echo off
setlocal

if defined JAVA_HOME goto findJavaFromJavaHome

set _JAVACMD=java
goto exec

:findJavaFromJavaHome
set _JAVACMD=%JAVA_HOME%\bin\java.exe

:exec
"%_JAVACMD%" -jar "%~dp0\gradle\wrapper\gradle-wrapper.jar" %*
