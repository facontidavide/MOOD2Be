#include <behaviortree_cpp/blackboard/blackboard_local.h>
#include <behaviortree_cpp/bt_factory.h>
#include <behaviortree_cpp/behavior_tree.h>
#include <behaviortree_cpp/xml_parsing.h>
#include <list>

#include <behaviortree_cpp/loggers/bt_cout_logger.h>
#include <behaviortree_cpp/loggers/bt_file_logger.h>
#include <behaviortree_cpp/loggers/bt_zmq_publisher.h>

#include "Intralogistic/args.hpp"
#include "Intralogistic/skill_interface.hpp"

using namespace BT;


int main(int argc, char** argv)
{   
    // Read more about args here: https://github.com/Taywee/args

    args::ArgumentParser parser("BehaviorTree.CPP Executor", "Load one or multiple plugins and the XML with the tree definition.");
    args::HelpFlag help(parser, "help", "Display this help menu", {'h', "help"});

    args::Group arguments(parser, "arguments", args::Group::Validators::DontCare, args::Options::Global);

    args::ValueFlag<std::string> tree_path(arguments, "path", "The XML containing the BehaviorTree ",
                                           {'t', "tree"}, args::Options::Required);

    args::ValueFlag<std::string> skills_path(arguments, "path", "JSON file containing the list of SmartSoft skills",
                                           {'s', "skills"}, args::Options::Required);

    args::ValueFlag<std::string> server_ip(arguments, "ip", "IP of the server",
                                           {"ip"}, "localhost" );

    try
    {
        parser.ParseCLI(argc, argv);
    }
    catch (const args::Completion& e)
    {
        std::cout << e.what();
        return 0;
    }
    catch (const args::Help&)
    {
        std::cout << parser;
        return 0;
    }
    catch (const args::ParseError& e)
    {
        std::cerr << e.what() << std::endl;
        std::cerr << parser;
        return 1;
    }
    catch (const args::RequiredError& e)
    {
        std::cerr << "One of the mandatory arguments is missing:" << std::endl;
        std::cerr << e.what() << std::endl;
        std::cerr << parser;
        return 1;
    }

    const std::string skills_file =  args::get(skills_path);
    const std::string tree_file   =  args::get(tree_path);
    const std::string IP =  args::get(server_ip);

    std::cout << "\ttree file: " << tree_file << std::endl;
    std::cout << "\tIP: " << IP << std::endl;
    std::cout << "\tskills file: " << skills_file << std::endl;

    //------------------------------------------------------
    // Populate the factory from Skills file and
    // create the tree from BehaviorTree file.

    zmq::context_t zmq_context(1);

    auto definitions = ParseSkillFile( skills_file );

    BehaviorTreeFactory factory;

    for (const auto& def: definitions )
    {
        auto creator = [def, &zmq_context, &IP](const std::string& name, const NodeParameters& params)
        {
            return std::unique_ptr<TreeNode>( new SkillAction(def, name, params, IP.c_str(), zmq_context) );
        };
    }

    auto blackboard = Blackboard::create<BlackboardLocal>();
    auto tree = BT::buildTreeFromFile(factory, tree_file, blackboard);

    // add loggers
    StdCoutLogger logger_cout(tree.root_node);
    FileLogger logger_file(tree.root_node, "ulm_trace.fbl");
    PublisherZMQ publisher_zmq(tree.root_node);

    //------------------------------------------------------
    // Execute the tree
    //while (1)
    {
        NodeStatus status = NodeStatus::RUNNING;
        // Keep on ticking until you get either a SUCCESS or FAILURE state
        while( status == NodeStatus::RUNNING)
        {
            status = tree.root_node->executeTick();
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
    return 0;
}
