#pragma once 

#include "result.h"

enum class ProjectType {
    APP, PLUGIN
};

class Project {
private:
    std::string name;
    std::string path;
    ProjectType type;
};

class ProjectManager {
public:
    static Result<Project> new_project(
        const std::string& project_name, 
        const std::string& project_path,
        ProjectType type);
};