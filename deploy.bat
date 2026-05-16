@echo off

set project_path=%CD%
set source_path=%project_path%\Builds\EdBuild\EdBuild.*
set destination_path=%project_path%\EdBuild.*

del %destination_path%
copy %source_path% %destination_path%