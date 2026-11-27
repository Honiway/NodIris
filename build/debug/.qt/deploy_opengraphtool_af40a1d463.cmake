include(/home/honiway/AbsoluteCoding/OpenGraphTool/build/debug/.qt/QtDeploySupport.cmake)
include("${CMAKE_CURRENT_LIST_DIR}/opengraphtool-plugins.cmake" OPTIONAL)
set(__QT_DEPLOY_I18N_CATALOGS "qtbase")

qt6_deploy_runtime_dependencies(
    EXECUTABLE /home/honiway/AbsoluteCoding/OpenGraphTool/build/debug/opengraphtool
    GENERATE_QT_CONF
)
