#include <vsg/all.h>

#ifdef vsgXchange_FOUND
#    include <vsgXchange/all.h>
#endif

#include <algorithm>
#include <chrono>
#include <iostream>
#include <thread>

struct Merge : public vsg::Inherit<vsg::Operation, Merge>
{
    Merge(const vsg::Path& in_path, vsg::observer_ptr<vsg::Viewer> in_viewer, vsg::ref_ptr<vsg::Group> in_attachmentPoint, vsg::ref_ptr<vsg::Node> in_node, const vsg::CompileResult& in_compileResult):
        path(in_path),
        viewer(in_viewer),
        attachmentPoint(in_attachmentPoint),
        node(in_node),
        compileResult(in_compileResult) {}

    vsg::Path path;
    vsg::observer_ptr<vsg::Viewer> viewer;
    vsg::ref_ptr<vsg::Group> attachmentPoint;
    vsg::ref_ptr<vsg::Node> node;
    vsg::CompileResult compileResult;

    void run() override
    {
        std::cout<<"Merge::run() path = "<<path<<", "<<attachmentPoint<<", "<<node<<std::endl;

        vsg::ref_ptr<vsg::Viewer> ref_viewer = viewer;
        if (ref_viewer)
        {
            updateViewer(*ref_viewer, compileResult);
        }

        attachmentPoint->addChild(node);
    }
};

struct LoadOperation : public vsg::Inherit<vsg::Operation, LoadOperation>
{
    LoadOperation(vsg::ref_ptr<vsg::Viewer> in_viewer, vsg::ref_ptr<vsg::Group> in_attachmentPoint, const vsg::Path& in_filename, vsg::ref_ptr<vsg::Options> in_options) :
        viewer(in_viewer),
        attachmentPoint(in_attachmentPoint),
        filename(in_filename),
        options(in_options) {}

    vsg::observer_ptr<vsg::Viewer> viewer;
    vsg::ref_ptr<vsg::Group> attachmentPoint;
    vsg::Path filename;
    vsg::ref_ptr<vsg::Options> options;

    void run() override
    {
        vsg::ref_ptr<vsg::Viewer > ref_viewer = viewer;

        // std::cout << "Loading " << filename << std::endl;
        if (auto node = vsg::read_cast<vsg::Node>(filename, options))
        {
            // std::cout << "Loaded " << filename << std::endl;

            vsg::ComputeBounds computeBounds;
            node->accept(computeBounds);

            vsg::dvec3 centre = (computeBounds.bounds.min + computeBounds.bounds.max) * 0.5;
            double radius = vsg::length(computeBounds.bounds.max - computeBounds.bounds.min) * 0.5;
            auto scale = vsg::MatrixTransform::create(vsg::scale(1.0 / radius, 1.0 / radius, 1.0 / radius) * vsg::translate(-centre));

            scale->addChild(node);

            auto result = ref_viewer->compileManager->compile(node);
            if (result) ref_viewer->addUpdateOperation(Merge::create(filename, viewer, attachmentPoint, scale, result));
        }
    }
};

int main(int argc, char** argv)
{
    try
    {
        // set up defaults and read command line arguments to override them
        vsg::CommandLine arguments(&argc, argv);

        // set up vsg::Options to pass in filepaths and ReaderWriter's and other IO related options to use when reading and writing files.
        auto options = vsg::Options::create();
        options->sharedObjects = vsg::SharedObjects::create();
        options->fileCache = vsg::getEnv("VSG_FILE_CACHE");
        options->paths = vsg::getEnvPaths("VSG_FILE_PATH");

#ifdef vsgXchange_all
        // add vsgXchange's support for reading and writing 3rd party file formats
        options->add(vsgXchange::all::create());
#endif

        arguments.read(options);

        auto windowTraits = vsg::WindowTraits::create();
        windowTraits->windowTitle = "vsgdynamicload";
        windowTraits->debugLayer = arguments.read({"--debug", "-d"});
        windowTraits->apiDumpLayer = arguments.read({"--api", "-a"});
        if (arguments.read({"--fullscreen", "--fs"})) windowTraits->fullscreen = true;
        if (arguments.read({"--window", "-w"}, windowTraits->width, windowTraits->height)) { windowTraits->fullscreen = false; }
        arguments.read("--screen", windowTraits->screenNum);
        arguments.read("--display", windowTraits->display);
        auto numFrames = arguments.value(-1, "-f");
        auto numThreads = arguments.value(16, "-n");

        // provide setting of the resource hints on the command line
        vsg::ref_ptr<vsg::ResourceHints> resourceHints;
        if (vsg::Path resourceFile; arguments.read("--resource", resourceFile)) resourceHints = vsg::read_cast<vsg::ResourceHints>(resourceFile);

        if (arguments.errors()) return arguments.writeErrorMessages(std::cerr);

        if (argc <= 1)
        {
            std::cout << "Please specify a 3d models on the command line." << std::endl;
            return 1;
        }

        // create a Group to contain all the nodes
        auto vsg_scene = vsg::Group::create();

        vsg::ref_ptr<vsg::Window> window(vsg::Window::create(windowTraits));
        if (!window)
        {
            std::cout << "Could not create windows." << std::endl;
            return 1;
        }


        // create the viewer and assign window(s) to it
        auto viewer = vsg::Viewer::create();

        viewer->addWindow(window);

        // set up the grid dimensions to place the loaded models on.
        vsg::dvec3 origin(0.0, 0.0, 0.0);
        vsg::dvec3 primary(2.0, 0.0, 0.0);
        vsg::dvec3 secondary(0.0, 2.0, 0.0);

        int numModels = static_cast<float>(argc - 1);
        int numColumns = static_cast<int>(std::ceil(std::sqrt(static_cast<float>(numModels))));
        int numRows = static_cast<int>(std::ceil(static_cast<float>(numModels) / static_cast<float>(numColumns)));

        // compute the bounds of the scene graph to help position camera
        vsg::dvec3 centre = origin + primary * (static_cast<double>(numColumns - 1) * 0.5) + secondary * (static_cast<double>(numRows - 1) * 0.5);
        double viewingDistance = std::sqrt(static_cast<float>(numModels)) * 3.0;
        double nearFarRatio = 0.001;

        // set up the camera
        auto lookAt = vsg::LookAt::create(centre + vsg::dvec3(0.0, -viewingDistance, 0.0), centre, vsg::dvec3(0.0, 0.0, 1.0));
        auto perspective = vsg::Perspective::create(30.0, static_cast<double>(window->extent2D().width) / static_cast<double>(window->extent2D().height), nearFarRatio * viewingDistance, viewingDistance * 2.0);
        auto viewportState = vsg::ViewportState::create(window->extent2D());
        auto camera = vsg::Camera::create(perspective, lookAt, viewportState);

        // add close handler to respond the close window button and pressing escape
        viewer->addEventHandler(vsg::CloseHandler::create(viewer));

        viewer->addEventHandler(vsg::Trackball::create(camera));

        auto commandGraph = vsg::createCommandGraphForView(window, camera, vsg_scene);
        viewer->assignRecordAndSubmitTaskAndPresentation({commandGraph});

        if (!resourceHints)
        {
            // To help reduce the number of vsg::DescriptorPool that need to be allocated we'll provide a minimum requirement via ResourceHints.
            resourceHints = vsg::ResourceHints::create();
            resourceHints->numDescriptorSets = 256;
            resourceHints->descriptorPoolSizes.push_back(VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 256});
        }

        // configure the viewers rendering backend, initialize and compile Vulkan objects, passing in ResourceHints to guide the resources allocated.
        viewer->compile(resourceHints);

        auto loadThreads = vsg::OperationThreads::create(numThreads, viewer->status);

        // assign the LoadOperation that will do the load in the background and once loaded and compiled merged then via Merge operation that is assigned to updateOperations and called from viewer.update()
        vsg::observer_ptr<vsg::Viewer> observer_viewer(viewer);
        for (int i = 1; i < argc; ++i)
        {
            int index = i - 1;
            vsg::dvec3 position = origin + primary * static_cast<double>(index % numColumns) + secondary * static_cast<double>(index / numColumns);
            auto transform = vsg::MatrixTransform::create(vsg::translate(position));

            vsg_scene->addChild(transform);

            loadThreads->add(LoadOperation::create(observer_viewer, transform, argv[i], options));
        }

        // rendering main loop
        while (viewer->advanceToNextFrame() && (numFrames < 0 || (numFrames--) > 0))
        {
            // std::cout<<"new Frame"<<std::endl;
            // pass any events into EventHandlers assigned to the Viewer
            viewer->handleEvents();

            viewer->update();

            viewer->recordAndSubmit();

            viewer->present();

            // if (loadThreads->queue->empty()) break;
        }
    }
    catch (const vsg::Exception& ve)
    {
        for (int i = 0; i < argc; ++i) std::cerr << argv[i] << " ";
        std::cerr << "\n[Exception] - " << ve.message << " result = " << ve.result << std::endl;
        return 1;
    }

    // clean up done automatically thanks to ref_ptr<>
    return 0;
}
