set(SOURCES
    GLMemoryExtensions.cpp
    GLSemaphore.cpp
    SharedTexture.cpp
    vsgwithosg.cpp
)

add_executable(vsgwithosg ${SOURCES})

target_include_directories(vsgwithosg PRIVATE ${OSG_INCLUDE_DIR})
target_link_libraries(vsgwithosg vsg::vsg ${OPENTHREADS_LIBRARIES} ${OSG_LIBRARIES} ${OSGUTIL_LIBRARIES} ${OSGDB_LIBRARIES} ${OSGGA_LIBRARIES} ${OSGVIEWER_LIBRARIES} ${OPENGL_LIBRARIES})

if (vsgXchange_FOUND)
    target_compile_definitions(vsgwithosg PRIVATE vsgXchange_FOUND)
    target_link_libraries(vsgwithosg vsgXchange::vsgXchange)
endif()
