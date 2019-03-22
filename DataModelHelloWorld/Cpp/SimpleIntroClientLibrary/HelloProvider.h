//**************************************************************************
//
// HelloProvider.h
// 
// Definition of a provider class which adds a new example property "Hello" to
// the debugger's notion of a process.
//
//**************************************************************************

namespace Debugger::DataModel::Libraries::Hello
{

using namespace Debugger::DataModel::ClientEx;
using namespace Debugger::DataModel::ProviderEx;

//*************************************************
// Internal Details:
//

namespace Details
{

// Hello:
//
// A C++ object which will be returned from a new "Hello" property on every process.
//
// [JavaScript: This (combined with HelloObject) is equivalent to the __HelloObject class]
// [COM       : This is equivalent to the HelloData class]
//
struct Hello
{
    std::wstring Text;
};

}

//*************************************************
// Extension Classes:
//

// HelloObject:
//
// A singleton class which makes instances of our internal "Hello" object accessible to the data model
//
// [JavaScript: This (combined with Details::Hello) is equivalent to the __HelloObject class]
// [COM       : This is equivalent to the data model created in HelloExtension::Initialize]
//
class HelloObject : public TypedInstanceModel<Details::Hello>
{
public:

    HelloObject();

    // Get_Test():
    //
    // The property accessor for an added example property on top of what is in the Details::Hello class.
    //
    Object Get_Test(_In_ const Object& helloInstance, _In_ const Details::Hello& hello);

    // GetStringConversion():
    //
    // The method which will be called to generate a string conversion of "Hello"
    //
    std::wstring GetStringConversion(_In_ const Object& helloInstance,
                                     _In_ Details::Hello& hello,
                                     _In_ const Metadata& metadata);


};

// HelloExtension:
//
// A singleton class which extends the debugger's notion of a process with a new "Hello" property.
//
// [JavaScript: This is equivalent to the __HelloExtension class]
// [COM       : This is equivalent to the HelloExtension and HelloExtensionModel classes]
//
class HelloExtension : public ExtensionModel
{
public:

    HelloExtension();

    // Get_Hello():
    //
    // The property accessor for the "Hello" property which is added to a process.
    //
    Details::Hello Get_Hello(_In_ const Object& processInstance);

};

// HelloProvider:
//
// A class which provides the "hello" set of functionality.  This is a singleton instance which encapsulates
// all of the extension classes and factories for this part of the debugger extension.
//
// [JavaScript: This (and ExtensionProvider) is represented by the overall script and the initializeScript method]
// [COM       : This is equivalent to the HelloExtension class]
//
class HelloProvider 
{
public:

    HelloProvider();
    ~HelloProvider();

    // GetHelloFactory():
    //
    // Gets our singeton instance of the class which makes Details::Hello visible to the data model.
    //
    HelloObject& GetHelloFactory() const { return *m_spHelloFactory.get(); }

    // Get():
    //
    // Gets the singleton instance of the HelloProvider.
    //
    static HelloProvider& Get()
    {
        return *s_pProvider;
    }

private:

    static HelloProvider *s_pProvider;

    // Factories:
    //
    // These are the classes which bridge C++ constructs (such as the Details::Hello object) to
    // the data model.
    //
    std::unique_ptr<HelloObject> m_spHelloFactory;

    // Extensions managed by this provider
    std::unique_ptr<HelloExtension> m_spHelloExtension;
};

} // Debugger::DataModel::Libraries::Hello

//*************************************************
// Custom Boxing and Unboxing of Internal Classes:
//

namespace Debugger::DataModel::ClientEx::Boxing
{

using namespace Debugger::DataModel::Libraries::Hello;
using namespace Debugger::DataModel::Libraries::Hello::Details;

template<>
struct BoxObject<Hello>
{
    static Hello Unbox(_In_ const Object& object);
    static Object Box(_In_ const Hello& hello);
};

};

