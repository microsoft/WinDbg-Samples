//**************************************************************************
//
// HelpStrings.h
//
// Resource identifiers for help strings.
//
//**************************************************************************
//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//**************************************************************************

#ifndef _HELPSTRINGS_H_
#define _HELPSTRINGS_H_

//*************************************************
// Extension Points:
//

#define SYMBOLBUILDER_IDS_MODULE_SYMBOLBUILDERSYMBOLS 100
#define SYMBOLBUILDER_IDS_CREATESYMBOLS 101

//*************************************************
// Type Related Objects:
//

//
// <SymbolSet>
//

#define SYMBOLBUILDER_IDS_SYMBOLSET_TYPES 200
#define SYMBOLBUILDER_IDS_SYMBOLSET_DATA 201

//
// <SymbolSet>.Types:
//

#define SYMBOLBUILDER_IDS_TYPES_ADDBASICCTYPES 300
#define SYMBOLBUILDER_IDS_TYPES_CREATE 301
#define SYMBOLBUILDER_IDS_TYPES_CREATEARRAY 302
#define SYMBOLBUILDER_IDS_TYPES_CREATEENUM 303
#define SYMBOLBUILDER_IDS_TYPES_CREATEPOINTER 304
#define SYMBOLBUILDER_IDS_TYPES_CREATETYPEDEF 305

//
// <Base Symbol>.*
//

#define SYMBOLBUILDER_IDS_SYMBOL_NAME 400
#define SYMBOLBUILDER_IDS_SYMBOL_QUALIFIEDNAME 401
#define SYMBOLBUILDER_IDS_SYMBOL_PARENT 402

//
// <Base Type>.*
//

#define SYMBOLBUILDER_IDS_TYPE_SIZE 500
#define SYMBOLBUILDER_IDS_TYPE_ALIGNMENT 501
#define SYMBOLBUILDER_IDS_TYPE_DELETE 502

//
// <UDT>.*
//

#define SYMBOLBUILDER_IDS_UDTTYPE_BASECLASSES 600
#define SYMBOLBUILDER_IDS_UDTTYPE_FIELDS 601

//
// <Pointer>.*
//

#define SYMBOLBUILDER_IDS_POINTERTYPE_BASETYPE 700

//
// <Array>.*
//

#define SYMBOLBUILDER_IDS_ARRAYTYPE_ARRAYSIZE 800
#define SYMBOLBUILDER_IDS_ARRAYTYPE_BASETYPE 801

//
// <Typedef>.*
//

#define SYMBOLBUILDER_IDS_TYPEDEFTYPE_BASETYPE 900

//
// <Enum>.*

#define SYMBOLBUILDER_IDS_ENUMTYPE_BASETYPE 1000
#define SYMBOLBUILDER_IDS_ENUMTYPE_ENUMERANTS 1001

//
// <Fields>.*
//

#define SYMBOLBUILDER_IDS_FIELDS_ADD 1100

//
// <Enumerants>.*
//

#define SYMBOLBUILDER_IDS_ENUMERANTS_ADD 1200

//
// <Field>.*
//

#define SYMBOLBUILDER_IDS_FIELD_ISAUTOMATICLAYOUT 1300
#define SYMBOLBUILDER_IDS_FIELD_TYPE 1301
#define SYMBOLBUILDER_IDS_FIELD_OFFSET 1302
#define SYMBOLBUILDER_IDS_FIELD_DELETE 1303
#define SYMBOLBUILDER_IDS_FIELD_MOVEBEFORE 1304

//
// <Base Classes>.*
//

#define SYMBOLBUILDER_IDS_BASECLASSES_ADD 1400

//
// <Base Class>.*
//

#define SYMBOLBUILDER_IDS_BASECLASS_ISAUTOMATICLAYOUT 1500
#define SYMBOLBUILDER_IDS_BASECLASS_TYPE 1501
#define SYMBOLBUILDER_IDS_BASECLASS_OFFSET 1502
#define SYMBOLBUILDER_IDS_BASECLASS_DELETE 1503
#define SYMBOLBUILDER_IDS_BASECLASS_MOVEBEFORE 1504

//
// <Data>.*
//

#define SYMBOLBUILDER_IDS_DATA_CREATEGLOBAL 2000

//
// <Global Data>.*
//

#define SYMBOLBUILDER_IDS_GLOBALDATA_TYPE 2100
#define SYMBOLBUILDER_IDS_GLOBALDATA_OFFSET 2101
#define SYMBOLBUILDER_IDS_GLOBALDATA_DELETE 2102

#endif // __HELPSTRINGS_H_
