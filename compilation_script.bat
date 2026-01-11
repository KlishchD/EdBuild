@echo off

set project_path=%CD%\
set intermediate_path=%project_path%\Intermediate\
set builds_path=%project_path%\Builds\

echo Project path is %project_path%
echo Intermediate path is %intermediate_path%
echo Builds path is %builds_path%

.\EdBuild.exe -Project %project_path% -Intermediate %intermediate_path% -Builds %builds_path% -Target D -Platform W -Threads 12