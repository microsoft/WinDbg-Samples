//**************************************************************************
//
// HelloExtension.h
//
// Properties and data models which extend the debugger's notion of what a process
// is to include new properties.
//
//**************************************************************************

// HelloData:
//
// The context data which backs the object returned from the 'Hello' property.
//
// [JavaScript: This is partially equivalent to the __HelloObject class]
// [C++17     : This is partially equivalent to the HelloObject and Details::Hello classes]
//
class HelloData :
	public Microsoft::WRL::RuntimeClass<
	Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::RuntimeClassType::ClassicCom>,
	IUnknown
	>
{
public:

	// RuntimeClassInitialize():
	//
	// Initializes this object.
	//
	HRESULT RuntimeClassInitialize(_In_ PCWSTR pwszText);

	// The text value that is associated with the object.
	std::wstring m_Text;
};

// TestProperty:
//
// A property accessor for the 'Test' property that is added to our 'World' type.
//
// [JavaScript: This is equivalent to the 'get Test()' property of the __HelloObject class]
// [C++17     : This is equivalent to the Get_Test method on the HelloObject class]
//
class TestProperty :
	public Microsoft::WRL::RuntimeClass<
	Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::RuntimeClassType::ClassicCom>,
	IModelPropertyAccessor
	>
{
public:

	IFACEMETHOD(GetValue)(_In_ PCWSTR pwszKey,
		_In_opt_ IModelObject *pProcessInstance,
		_COM_Outptr_ IModelObject **ppValue);

	// SetValue():
	//
	// Sets the value of the 'Test' property.  This is a read only property and hence
	// the method returns E_NOTIMPL.
	//
	IFACEMETHOD(SetValue)(_In_ PCWSTR /*pwszKey*/,
		_In_opt_ IModelObject * /*pProcessInstance*/,
		_In_ IModelObject * /*pValue*/)
	{
		return E_NOTIMPL;
	}

};

// WorldProperty:
//
// A property accessor for the 'World' property that is added to our 'World' type.
//
// [JavaScript: This is equivalent to the 'get World()' property of the __HelloObject class]
// [C++17     : This is implemented via data binding in the HelloObject constructor]
//
class WorldProperty :
	public Microsoft::WRL::RuntimeClass<
	Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::RuntimeClassType::ClassicCom>,
	IModelPropertyAccessor
	>
{
public:

	IFACEMETHOD(GetValue)(_In_ PCWSTR pwszKey,
		_In_opt_ IModelObject *pProcessInstance,
		_COM_Outptr_ IModelObject **ppValue);

	// SetValue():
	//
	// Sets the value of the 'World' property.  This is a read only property and hence
	// the method returns E_NOTIMPL.
	//
	IFACEMETHOD(SetValue)(_In_ PCWSTR /*pwszKey*/,
		_In_opt_ IModelObject * /*pProcessInstance*/,
		_In_ IModelObject * /*pValue*/)
	{
		return E_NOTIMPL;
	}

	// RuntimeClassInitialize():
	//
	// Initialies the property.
	//
	HRESULT RuntimeClassInitialize(_In_ IModelObject *pDataModel)
	{
		m_pDataModel = pDataModel;
		return S_OK;
	}

private:

	// Weak back pointer to the data model that contains this property
	IModelObject *m_pDataModel;

};

// HelloModel:
//
// The IDataModelConcept implementation for the data model which acts much like a "type" for the object
// we are returning from the 'Hello' property.
//
// [JavaScript: This is partially equivalent to the __HelloObject class]
// [C++17     : This is partially equivalent to the HelloObject class]
//
class HelloModel :
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

// HelloProperty:
//
// A property accessor for the 'Hello' property that is added to the notion of a process.
//
// [JavaScript: This is equivalent to the 'get Hello()' property of the __HelloExtension class]
// [C++17     : This is equivalent to the Get_Hello method on the HelloExtension class]
//
class HelloProperty :
	public Microsoft::WRL::RuntimeClass<
	Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::RuntimeClassType::ClassicCom>,
	IModelPropertyAccessor
	>
{
public:

	IFACEMETHOD(GetValue)(_In_ PCWSTR pwszKey,
		_In_opt_ IModelObject *pProcessInstance,
		_COM_Outptr_ IModelObject **ppValue);

	// SetValue():
	//
	// Sets the value of the 'Hello' property.  This is a read only property and hence
	// the method returns E_NOTIMPL.
	//
	IFACEMETHOD(SetValue)(_In_ PCWSTR /*pwszKey*/,
		_In_opt_ IModelObject * /*pProcessInstance*/,
		_In_ IModelObject * /*pValue*/)
	{
		return E_NOTIMPL;
	}

	// RuntimeClassInitialize():
	//
	// Initialies the property.
	//
	HRESULT RuntimeClassInitialize();

private:

	// The data model we create to represent the type we return.  This could be stored more globally
	// than the property accessor if it's used elsewhere.
	Microsoft::WRL::ComPtr<IModelObject> m_spHelloType;

};

// HelloStringConversion:
//
// The IStringDisplayableConcept implementation for the Hello model which allows an instance of that model
// to be converted to a display string for the debugger.
//
// [JavaScript: This is equivalent to the toString method on the __HelloObject class]
// [C++17     : This is equivalent to the GetStringConversion method on the HelloObject class]
//
class HelloStringConversion :
	public Microsoft::WRL::RuntimeClass<
	Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::RuntimeClassType::ClassicCom>,
	IStringDisplayableConcept
	>
{
public:

	// ToDisplayString():
	//
	// Converts an instance of the "Hello" object to a display string.
	//
	IFACEMETHOD(ToDisplayString)(_In_ IModelObject *pHelloInstance,
		_In_opt_ IKeyStore *pMetadata,
		_Out_ BSTR *pDisplayString);

	// RuntimeClassInitialize():
	//
	// Initializes the string conversion.
	//
	HRESULT RuntimeClassInitialize(_In_ IModelObject *pDataModel)
	{
		m_pDataModel = pDataModel;
		return S_OK;
	}

private:

	// Weak back pointer to the data model that contains this concept
	IModelObject *m_pDataModel;
};

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

// HelloExtension:
//
// A collection of the extensibility points that this extension places on the debugger's notion of a process.
// 
// [JavaScript: This is represented by the script itself and the initializeScript method]
// [C++17     : This is equivalent to the HelloProvider and ExtensionProvider classes]
//
class HelloExtension
{
public:

	~HelloExtension()
	{
		Uninitialize();
	}

	HRESULT Initialize();
	void Uninitialize();

private:

	Microsoft::WRL::ComPtr<IModelObject> m_spProcessModelObject;
	Microsoft::WRL::ComPtr<IModelObject> m_spHelloExtensionModelObject;

};
