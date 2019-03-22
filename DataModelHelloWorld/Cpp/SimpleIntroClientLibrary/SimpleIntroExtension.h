//**************************************************************************
//
// SimpleIntroExtension.h
//
// A sample C++ debugger extension which adds a new example property "Hello"
// to the debugger's notion of a process.
//
//**************************************************************************

namespace Debugger::DataModel::Libraries
{

using namespace Debugger::DataModel::ClientEx;
using namespace Debugger::DataModel::ProviderEx;

// ExtensionProvider():
//
// A singleton which 
// A provider class which links together all the individual provider libraries into one thing the engine
// extension can deal with.
//
class ExtensionProvider
{
public:

    // ExtensionProvider():
    //
    // Construct the provider for this extension and instantiate any individual extension classes which
    // together form the functionality of this debugger extension.
    //
    ExtensionProvider();

    Hello::HelloProvider& GetHelloProvider() const { return *m_spHelloProvider.get(); }

private:

    std::unique_ptr<Hello::HelloProvider> m_spHelloProvider;
};

} // Debugger::DataModel::Libraries

