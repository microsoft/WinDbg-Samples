"use strict";

// delete Object.prototype.toString;

//*******************************************************************************
// Python 3.11 Extension Script
//
// You must have a PDB for the python DLL in order for this to function properly.  Note that in order
// to speed usage of this module, the extension assumes the debugger is targeting a single process
// with a single version of Python.  It may not operate properly when debugging multiple processes
// particularly if differing version of embedded Python are utilized.
//

// Cached copy of the python module in which to look up key symbols.
class __PythonCache
{
    constructor()
    {
        this.__pythonModule = null;
        this.__tyCache = null;
        this.__pointerSize = 0;
    }

    get pythonModule()
    {
        if (this.__pythonModule == null)
        {
            var process = host.currentProcess;
            for (var mod of process.Modules)
            {
                // .Name may, unfortunately, include a path.  Strip it.
                var modName = mod.Name;
                modName = modName.substring(modName.lastIndexOf("\\") + 1);
                if (modName.startsWith("python"))
                {
                    this.__pythonModule = mod;
                    break;
                }
            }
        }

        return this.__pythonModule;
    }

    get pointerSize()
    {
        if (this.__pointerSize == 0)
        {
            var voidPtr = host.getModuleType(this.pythonModule, "void *");
            this.__pointerSize = voidPtr.size;
        }
        return this.__pointerSize;
    }

    // ToDynamicType():
    //
    // Converts obj to its dynamic type (from a generic object header to a specific object like PyDictObject).  If the
    // method cannot make the determination, the original object is returned.
    //
    ToDynamicType(obj)
    {
        var objTy = obj.ob_type;
        var tyAddr = objTy.address;
        var dynType = this.__typeCache[tyAddr];
        if (dynType === undefined)
        {
            return obj;
        }

        var newTy = host.getModuleType(this.pythonModule, dynType);
        var newObj = host.createTypedObject(obj.address, newTy);
        return newObj;
    }

    //*************************************************
    // Internal Cache Helpers:
    //

    get __typeCache()
    {
        if (this.__tyCache == null)
        {
            this.__CacheTypes();
        }

        return this.__tyCache;
    }

    __CacheTypes()
    {
        if (this.__tyCache == null)
        {
            this.__tyCache = { };
            this.__CacheType("PyDict_Type", "PyDictObject");
            this.__CacheType("PyUnicode_Type", "PyUnicodeObject");
            this.__CacheType("PyLong_Type", "PyLongObject");
            this.__CacheType("PyProperty_Type", "propertyobject");
            this.__CacheType("PyFunction_Type", "PyFunctionObject");
            this.__CacheType("PyTuple_Type", "PyTupleObject");
            this.__CacheType("PyType_Type", "PyTypeObject");
            this.__CacheType("PyGetSetDescr_Type", "PyGetSetDescrObject");
            this.__CacheType("PyMethodDescr_Type", "PyMethodDescrObject");
            this.__CacheType("PyWrapperDescr_Type", "PyWrapperDescrObject");
        }
    }

    __CacheType(obj, tyName)
    {
        var typeObj = host.getModuleSymbol(__getCache().pythonModule, obj);
        this.__tyCache[typeObj.address] = tyName;
    }
}

var __cache = new __PythonCache();

function __getCache()
{
    return __cache;
}

// __KeyValuePair:
//
// A key value pair.
//
class __KeyValuePair
{
    constructor(key, value)
    {
        this.key = key;
        this.value = value;
    }

    toString()
    {
        var kder = this.key.runtimeTypedObject;
        var vder = this.value.runtimeTypedObject;
        return "{ " + kder.toString() + " = " + vder.toString() + " }";
    }
}

// __PyDictObjectVisualizer:
//
// A visualizer for a python dictionary.
//
class __PyDictObjectVisualizer
{
    toString()
    {
        return "<Object of type dict>";
    }

    get __index_size()
    {
        return 1 << this.ma_keys.dk_log2_size;
    }

    get __index_byte_size()
    {
        return 1 << this.ma_keys.dk_log2_index_bytes;
    }

    // __index_entry_size():
    //
    // Returns the size of an entry in the hash index (in bytes)
    //
    get __index_entry_size()
    {
        var idxSizeLog2 = this.ma_keys.dk_log2_size;
        if (idxSizeLog2 < 8)
        {
            return 1;
        }
        else if (idxSizeLog2 < 16)
        {
            return 2;
        }
        else if (idxSizeLog2 >= 32 && __getCache().pointerSize > 4)
        {
            return 8;
        }
        else
        {
            return 4;
        }
    }

    get __isSplitTable()
    {
        return !this.ma_values.isNull || this.dk_kind == 2 /* DICT_KEYS_SPLIT */;
    }

    __isValidIndex(idx)
    {
        return idx < this.__index_size;
    }

    get __DK_ENTRIES()
    {
        var entriesAddr = this.ma_keys.dk_indices.address.add(this.__index_byte_size);
        return entriesAddr;
    }

    __getKeyAt(idx)
    {
        if (this.__isSplitTable)
        {
            var entryAddr = this.__DK_ENTRIES.add(idx * __getCache().pointerSize);
            var ptrVal = host.memory.readMemoryValues(entryAddr, 1, __getCache().pointerSize, false, this) [0];
            var keyObj = host.createTypedObject(ptrVal, __getCache().pythonModule, "_object");
            return keyObj;
        }
        
        var ty = this.__entryType;
        var entryAddr = this.__DK_ENTRIES.add(idx * ty.size);
        var entry = host.createTypedObject(entryAddr, ty, this);
        return entry.me_key;
    }

    __getValueAt(idx)
    {
        if (this.__isSplitTable)
        {
            return this.ma_values.values.getValueAt(idx).dereference();
        }

        var ty = this.__entryType;
        var entryAddr = this.__DK_ENTRIES.add(idx * ty.size);
        var entry = host.createTypedObject(entryAddr, ty, this);
        return entry.me_value;
    }

    __entries_address()
    {
        return this.ma_keys.dk_indicies.address.add(this.__index_byte_size);
    }

    get __entryType()
    {
        if (this.ma_keys.dk_kind == 1 /* DICT_KEYS_UNICODE */)
        {
            return host.getModuleType(__getCache().pythonModule, "PyDictUnicodeEntry");
        }
        else if (this.ma_keys.dk_kind == 0 /* DICT_KEYS_GENERAL */)
        {
            return host.getModuleType(__getCache().pythonModule, "PyDictKeyEntry");
        }

        throw new Error("This should not be called on a split dictionary");
    }

    *[Symbol.iterator]()
    {
        var idx = host.memory.readMemoryValues(this.ma_keys.dk_indices.address, this.__index_size, this.__index_entry_size, false, this);
        for (var idxVal of idx)
        {
            if (this.__isValidIndex(idxVal))
            {
                var key = this.__getKeyAt(idxVal);
                var val = this.__getValueAt(idxVal);
                yield new __KeyValuePair(key, val);
            }
        }
    }
}

// __PyUnicodeObjectVisualizer:
//
// A visualizer for a python unicode object.
//
class __PyUnicodeObjectVisualizer
{
    toString()
    {
        var state = this._base._base.state;

        // PyASCIIObject (compact ascii):
        if (state.ascii != 0 && state.compact != 0 && state.ready != 0)
        {
            var asciiObj = this._base._base;
            var dataAddr = asciiObj.address.add(asciiObj.targetType.size);
            return host.memory.readString(dataAddr, this);
        }

        return "<NOT YET IMPLEMENTED>";
   }
}

// __PyLongObjectVisualizer:
//
// A visualizer for a python long object.
//
class __PyLongObjectVisualizer
{
    toString(radix)
    {
        //
        // @TODO: We should do this generically as Python's longs are arbitrary precision and this limits us to 64 bits...
        //
        var str = this.Value.toString();
        return str;
    }

    get Value()
    {
        var digitTySize = this.ob_digit.targetType.baseType.size;
        var digitSize = (digitTySize >= 30) ? 30 : 15;

        var val = host.Int64(0, 0);

        var size = this.ob_base.ob_size;
        var digitsAddr = this.ob_digit.address;
        var digits = host.memory.readMemoryValues(digitsAddr, size, (digitSize == 30) ? 4 : 2, false, this);

        for (var digit of digits)
        {
            val = val.bitwiseShiftLeft(digitSize);
            val = val.bitwiseOr(digit);
        }

        return val;
    }
}

// __PyTupleObjectVisualizer:
//
// A visualizer for a python tuple object
//
class __PyTupleObjectVisualizer
{
    *[Symbol.iterator]()
    {
        var size = this.ob_base.ob_size;
        var objects = this.ob_item;
        for (var i = 0; i < size; ++i)
        {
            var obj = objects.getValueAt(i);
            yield obj.dereference();
        }
    }
}

// __PyTypeObjectVisualizer:
//
// A visualizer for a python type object
//
class __PyTypeObjectVisualizer
{
    toString()
    {
        var name = host.memory.readString(this.tp_name, this);
        return "< Type object for type '" + name + "' >";
    }
}

// __PyGetSetDescrObjectVisualizer:
//
// A visualizer for a python get/set descriptor object
//
class __PyGetSetDescrObjectVisualizer
{
    toString()
    {
        var descrTypeName = host.memory.readString(this.d_common.d_type.tp_name);
        var propName = "UNKNOWN";
        var d_name = this.d_common.d_name;
        if (!d_name.isNull)
        {
            propName = d_name.dereference().runtimeTypedObject.toString();
        }
        
        return "< PyGetSetDescrObject for '" + propName + "' in '" + descrTypeName + "' >";
    }
}

// __PyMethodDescrObjectVisualizer:
//
// A visualizer for a python method descriptor object
//
class __PyMethodDescrObjectVisualizer
{
    toString()
    {
        var descrTypeName = host.memory.readString(this.d_common.d_type.tp_name);
        var methName = "UNKNOWN";
        var d_name = this.d_common.d_name;
        if (!d_name.isNull)
        {
            methName = d_name.dereference().runtimeTypedObject.toString();
        }
        
        return "< PyMethodDescrObject for '" + methName + "' in '" + descrTypeName + "' >";
    }

}

class __genStringViz
{
    toString()
    {
        var tyName = host.memory.readString(this.ob_base.ob_type.tp_name, this);
        return "<Object of type " + tyName + " >";
    }
}

class __ObjectVisualizer
{
    toString()
    {
        var tyName = host.memory.readString(this.ob_type.tp_name, this);
        return "<Object of type " + tyName + ">";
    }

    getPreferredRuntimeTypedObject()
    {
        return __getCache().ToDynamicType(this);
    }
}

// __PythonException:
//
// A wrapper around a Python exception given by type, value, and traceback.
//
class __PythonException
{
    constructor(excType, excValue, excTraceback)
    {
        this.Type = excType;
        this.Value = excValue;
        this.Traceback = excTraceback;
    }

    toString()
    {
        var str = this.Type.dereference().runtimeTypedObject.toString();
        if (!this.Value.isNull)
        {
            str += " ";
            str += this.Value.dereference().runtimeTypedObject.toString();
        }
        return str;
    }
}


// __PythonProcessExtension:
//
// An extension of the process object (the <process>.Python namespace)
//
class __PythonProcessExtension
{
    get Runtime()
    {
        return host.getModuleSymbol(__getCache().pythonModule, "_PyRuntime", this);
    }

    get CurrentThreadState()
    {
        return host.createTypedObject(this.Python.Runtime.gilstate.tstate_current._value, __getCache().pythonModule, "PyThreadState", this);
    }

    get CurrentException()
    {
        var threadState = this.Python.CurrentThreadState;
        if (threadState.address.compareTo(0) == 0 || threadState.curexc_type.isNull)
        {
            return null;
        }

        return new __PythonException(threadState.curexc_type, threadState.curexc_value, threadState.curexc_traceback);
    }
}

function initializeScript()
{
    return [new host.apiVersionSupport(1, 7),
            new host.typeSignatureRegistration(__ObjectVisualizer, "_object"),
            new host.typeSignatureRegistration(__PyDictObjectVisualizer, "PyDictObject"),
            new host.typeSignatureRegistration(__PyUnicodeObjectVisualizer, "PyUnicodeObject"),
            new host.typeSignatureRegistration(__PyLongObjectVisualizer, "PyLongObject"),
            new host.typeSignatureRegistration(__PyTupleObjectVisualizer, "PyTupleObject"),
            new host.typeSignatureExtension(__genStringViz, "propertyobject"),
            new host.typeSignatureExtension(__genStringViz, "PyFunctionObject"),
            new host.typeSignatureExtension(__PyMethodDescrObjectVisualizer, "PyMethodDescrObject"),
            new host.typeSignatureExtension(__PyGetSetDescrObjectVisualizer, "PyGetSetDescrObject"),
            new host.typeSignatureExtension(__PyTypeObjectVisualizer, "PyTypeObject"),
            new host.namespacePropertyParent(__PythonProcessExtension, "Debugger.Models.Process", "Debugger.Models.Process.Python", "Python")];
}
