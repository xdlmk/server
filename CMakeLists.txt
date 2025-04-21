cmake_minimum_required(VERSION 3.14)

project(Server LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core Network Protobuf Sql Gui)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Network Protobuf Sql Gui)

qt_add_protobuf(protosource
    PROTO_FILES
        ${CMAKE_SOURCE_DIR}/proto/envelope/envelope.proto
        ${CMAKE_SOURCE_DIR}/proto/auth/login.proto
        ${CMAKE_SOURCE_DIR}/proto/auth/register.proto
        ${CMAKE_SOURCE_DIR}/proto/search/search.proto
        ${CMAKE_SOURCE_DIR}/proto/profile/editProfile.proto
        ${CMAKE_SOURCE_DIR}/proto/avatars/avatarsUpdate.proto
        ${CMAKE_SOURCE_DIR}/proto/avatars/getAvatar.proto
        ${CMAKE_SOURCE_DIR}/proto/common/identifiers.proto
        ${CMAKE_SOURCE_DIR}/proto/chats/chatsInfo.proto
        ${CMAKE_SOURCE_DIR}/proto/chats/updatingChats.proto
        ${CMAKE_SOURCE_DIR}/proto/chats/chatMessage.proto
        ${CMAKE_SOURCE_DIR}/proto/chats/loadMessages.proto
        ${CMAKE_SOURCE_DIR}/proto/group/createGroup.proto
        ${CMAKE_SOURCE_DIR}/proto/group/deleteMember.proto
        ${CMAKE_SOURCE_DIR}/proto/group/addMembers.proto
    OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/generated_protobuf"
)

include_directories(${CMAKE_CURRENT_BINARY_DIR})

add_executable(Server
    main.cpp
    messageServer/chatnetworkmanager.h messageServer/chatnetworkmanager.cpp
    messageServer/clienthandler.h messageServer/clienthandler.cpp
    messageServer/messageprocessor.h messageServer/messageprocessor.cpp

    messageServer/Database/databaseconnector.h messageServer/Database/databaseconnector.cpp
    messageServer/Database/usermanager.h messageServer/Database/usermanager.cpp
    messageServer/Database/chatmanager.h messageServer/Database/chatmanager.cpp
    messageServer/Database/groupmanager.h messageServer/Database/groupmanager.cpp
    messageServer/Database/filemanager.h messageServer/Database/filemanager.cpp

    fileServer/fileserver.h fileServer/fileserver.cpp
    fileServer/filehandler.h fileServer/filehandler.cpp
    fileServer/fileclienthandler.h fileServer/fileclienthandler.cpp

    Utils/logger.h Utils/logger.cpp
)

add_dependencies(Server protosource)

target_link_libraries(Server
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Network
    Qt${QT_VERSION_MAJOR}::Protobuf
    Qt${QT_VERSION_MAJOR}::Sql
    Qt${QT_VERSION_MAJOR}::Gui
    protosource
)

include(GNUInstallDirs)
install(TARGETS Server
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
