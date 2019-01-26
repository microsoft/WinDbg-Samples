//**************************************************************************
//
// HelloExtension.h
//
// Properties and data models which extend the debugger's notion of what a process
// is to include new properties.
//
//**************************************************************************

// HelloExtensionModel:
//
// The IDataModelConcept implementation for our data model which extends process.  Every object which is
// attached to another in the parent model hierarchy must implement the IDataModelConcept concept.
//
// [JavaScript: This is partially equivalent to the __HelloExtension class]
// [C++17: This is partially equivalent to the HelloExtension class]
//
class HelloExtensionModel :
	public Microsoft::WRL::RuntimeClass<
	Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::RuntimeClassType::ClassicCom>,
	IDataModelConcept
	>
{
public:

	// InitializeObject():
	//
	// If the model is attached to a native object through a type signature, this method will be called
	// on each data model to indicate which type signature matched and what concrete symbols matched
	// wildcards within the type signature.  This method provides an opportunity for implementations to
	// cache attributes of the match.
	//
	IFACEMETHOD(InitializeObject)(_In_ IModelObject * /*pObject*/,
		_In_opt_ IDebugHostTypeSignature * /*pMatchingTypeSignature*/,
		_In_opt_ IDebugHostSymbolEnumerator * /*pWildcardMatches*/)
	{
		return S_OK;
	}

	// GetName():
	//
	// If this model exposes itself as an extensibility point (similar to "Debugger.Models.Process"), this
	// must return the name the model is registered under; otherwise, the method returns E_NOTIMPL.
	// 
	IFACEMETHOD(GetName)(_Out_ BSTR *pModelName)
	{
		*pModelName = nullptr;
		return E_NOTIMPL;
	}
};