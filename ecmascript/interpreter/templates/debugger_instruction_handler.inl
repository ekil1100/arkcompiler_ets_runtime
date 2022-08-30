/*
 * Copyright (c) 2021 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

    HANDLE_OPCODE(DEBUG_HANDLE_LDNAN_PREF)
    {
        NOTIFY_DEBUGGER_EVENT();
        REAL_GOTO_DISPATCH_OPCODE(EcmaOpcode::LDNAN_PREF);
    }
    HANDLE_OPCODE(DEBUG_HANDLE_LDINFINITY_PREF)
    {
        NOTIFY_DEBUGGER_EVENT();
        REAL_GOTO_DISPATCH_OPCODE(EcmaOpcode::LDINFINITY_PREF);
    }
    HANDLE_OPCODE(DEBUG_HANDLE_LDGLOBALTHIS_PREF)
    {
        NOTIFY_DEBUGGER_EVENT();
        REAL_GOTO_DISPATCH_OPCODE(EcmaOpcode::LDGLOBALTHIS_PREF);
    }
    HANDLE_OPCODE(DEBUG_HANDLE_LDUNDEFINED_PREF)
    {
        NOTIFY_DEBUGGER_EVENT();
        REAL_GOTO_DISPATCH_OPCODE(EcmaOpcode::LDUNDEFINED_PREF);
    }
    HANDLE_OPCODE(DEBUG_HANDLE_LDNULL_PREF)
    {
        NOTIFY_DEBUGGER_EVENT();
        REAL_GOTO_DISPATCH_OPCODE(EcmaOpcode::LDNULL_PREF);
    }
    HANDLE_OPCODE(DEBUG_HANDLE_LDSYMBOL_PREF)
    {
        NOTIFY_DEBUGGER_EVENT();
        REAL_GOTO_DISPATCH_OPCODE(EcmaOpcode::LDSYMBOL_PREF);
    }
    HANDLE_OPCODE(DEBUG_HANDLE_LDGLOBAL_PREF)
    {
        NOTIFY_DEBUGGER_EVENT();
        REAL_GOTO_DISPATCH_OPCODE(EcmaOpcode::LDGLOBAL_PREF);
    }
    HANDLE_OPCODE(DEBUG_HANDLE_LDTRUE_PREF)
    {
        NOTIFY_DEBUGGER_EVENT();
        REAL_GOTO_DISPATCH_OPCODE(EcmaOpcode::LDTRUE_PREF);
    }
    HANDLE_OPCODE(DEBUG_HANDLE_LDFALSE_PREF)
    {
        NOTIFY_DEBUGGER_EVENT();
        REAL_GOTO_DISPATCH_OPCODE(EcmaOpcode::LDFALSE_PREF);
    }
    HANDLE_OPCODE(DEBUG_HANDLE_THROWDYN_PREF)
    {
        NOTIFY_DEBUGGER_EVENT();
        REAL_GOTO_DISPATCH_OPCODE(EcmaOpcode::THROWDYN_PREF);
    }
    HANDLE_OPCODE(DEBUG_HANDLE_TYPEOFDYN_PREF)
    {
        NOTIFY_DEBUGGER_EVENT();
        REAL_GOTO_DISPATCH_OPCODE(EcmaOpcode::TYPEOFDYN_PREF);
    }
    HANDLE_OPCODE(DEBUG_HANDLE_LDLEXENVDYN_PREF)
    {
        NOTIFY_DEBUGGER_EVENT();
        REAL_GOTO_DISPATCH_OPCODE(EcmaOpcode::LDLEXENVDYN_PREF);
    }
    HANDLE_OPCODE(DEBUG_HANDLE_POPLEXENVDYN_PREF)
    {
        NOTIFY_DEBUGGER_EVENT();
        REAL_GOTO_DISPATCH_OPCODE(EcmaOpcode::POPLEXENVDYN_PREF);
    }
    HANDLE_OPCODE(DEBUG_HANDLE_GETUNMAPPEDARGS_PREF)
    {
        NOTIFY_DEBUGGER_EVENT();
        REAL_GOTO_DISPATCH_OPCODE(EcmaOpcode::GETUNMAPPEDARGS_PREF);
    }
    HANDLE_OPCODE(DEBUG_HANDLE_GETPROPITERATOR_PREF)
    {
        NOTIFY_DEBUGGER_EVENT();
        REAL_GOTO_DISPATCH_OPCODE(EcmaOpcode::GETPROPITERATOR_PREF);
    }
    HANDLE_OPCODE(DEBUG_HANDLE_ASYNCFUNCTIONENTER_PREF)
    {
        NOTIFY_DEBUGGER_EVENT();
        REAL_GOTO_DISPATCH_OPCODE(EcmaOpcode::ASYNCFUNCTIONENTER_PREF);
    }
    HANDLE_OPCODE(DEBUG_HANDLE_LDHOLE_PREF)
    {
        NOTIFY_DEBUGGER_EVENT();
        REAL_GOTO_DISPATCH_OPCODE(EcmaOpcode::LDHOLE_PREF);
    }
    HANDLE_OPCODE(DEBUG_HANDLE_RETURNUNDEFINED_PREF)
    {
        NOTIFY_DEBUGGER_EVENT();
        REAL_GOTO_DISPATCH_OPCODE(EcmaOpcode::RETURNUNDEFINED_PREF);
    }
    HANDLE_OPCODE(DEBUG_HANDLE_CREATEEMPTYOBJECT_PREF)
    {
        NOTIFY_DEBUGGER_EVENT();
        REAL_GOTO_DISPATCH_OPCODE(EcmaOpcode::CREATEEMPTYOBJECT_PREF);
    }
    HANDLE_OPCODE(DEBUG_HANDLE_CREATEEMPTYARRAY_PREF)
    {
        NOTIFY_DEBUGGER_EVENT();
        REAL_GOTO_DISPATCH_OPCODE(EcmaOpcode::CREATEEMPTYARRAY_PREF);
    }
    HANDLE_OPCODE(DEBUG_HANDLE_GETITERATOR_PREF)
    {
        NOTIFY_DEBUGGER_EVENT();
        REAL_GOTO_DISPATCH_OPCODE(EcmaOpcode::GETITERATOR_PREF);
    }
    HANDLE_OPCODE(DEBUG_HANDLE_THROWTHROWNOTEXISTS_PREF)
    {
        NOTIFY_DEBUGGER_EVENT();
        REAL_GOTO_DISPATCH_OPCODE(EcmaOpcode::THROWTHROWNOTEXISTS_PREF);
    }
    HANDLE_OPCODE(DEBUG_HANDLE_THROWPATTERNNONCOERCIBLE_PREF)
    {
        NOTIFY_DEBUGGER_EVENT();
        REAL_GOTO_DISPATCH_OPCODE(EcmaOpcode::THROWPATTERNNONCOERCIBLE_PREF);
    }
    HANDLE_OPCODE(DEBUG_HANDLE_LDHOMEOBJECT_PREF)
    {
        NOTIFY_DEBUGGER_EVENT();
        REAL_GOTO_DISPATCH_OPCODE(EcmaOpcode::LDHOMEOBJECT_PREF);
    }
    HANDLE_OPCODE(DEBUG_HANDLE_THROWDELETESUPERPROPERTY_PREF)
    {
        NOTIFY_DEBUGGER_EVENT();
        REAL_GOTO_DISPATCH_OPCODE(EcmaOpcode::THROWDELETESUPERPROPERTY_PREF);
    }
    HANDLE_OPCODE(DEBUG_HANDLE_DEBUGGER_PREF)
    {
        NOTIFY_DEBUGGER_EVENT();
        REAL_GOTO_DISPATCH_OPCODE(EcmaOpcode::DEBUGGER_PREF);
    }
    HANDLE_OPCODE(DEBUG_HANDLE_ADD2DYN_PREF_V8)
    {
        NOTIFY_DEBUGGER_EVENT();
        REAL_GOTO_DISPATCH_OPCODE(EcmaOpcode::ADD2DYN_PREF_V8);
    }
    HANDLE_OPCODE(DEBUG_HANDLE_SUB2DYN_PREF_V8)
    {
        NOTIFY_DEBUGGER_EVENT();
        REAL_GOTO_DISPATCH_OPCODE(EcmaOpcode::SUB2DYN_PREF_V8);
    }
    HANDLE_OPCODE(DEBUG_HANDLE_MUL2DYN_PREF_V8)
    {
        NOTIFY_DEBUGGER_EVENT();
        REAL_GOTO_DISPATCH_OPCODE(EcmaOpcode::MUL2DYN_PREF_V8);
    }
    HANDLE_OPCODE(DEBUG_HANDLE_DIV2DYN_PREF_V8)
    {
        NOTIFY_DEBUGGER_EVENT();
        REAL_GOTO_DISPATCH_OPCODE(EcmaOpcode::DIV2DYN_PREF_V8);
    }
    HANDLE_OPCODE(DEBUG_HANDLE_MOD2DYN_PREF_V8)
    {
        NOTIFY_DEBUGGER_EVENT();
        REAL_GOTO_DISPATCH_OPCODE(EcmaOpcode::MOD2DYN_PREF_V8);
    }
    HANDLE_OPCODE(DEBUG_HANDLE_EQDYN_PREF_V8)
    {
        NOTIFY_DEBUGGER_EVENT();
        REAL_GOTO_DISPATCH_OPCODE(EcmaOpcode::EQDYN_PREF_V8);
    }
    HANDLE_OPCODE(DEBUG_HANDLE_NOTEQDYN_PREF_V8)
    {
        NOTIFY_DEBUGGER_EVENT();
        REAL_GOTO_DISPATCH_OPCODE(EcmaOpcode::NOTEQDYN_PREF_V8);
    }
    HANDLE_OPCODE(DEBUG_HANDLE_LESSDYN_PREF_V8)
    {
        NOTIFY_DEBUGGER_EVENT();
        REAL_GOTO_DISPATCH_OPCODE(EcmaOpcode::LESSDYN_PREF_V8);
    }
    HANDLE_OPCODE(DEBUG_HANDLE_LESSEQDYN_PREF_V8)
    {
        NOTIFY_DEBUGGER_EVENT();
        REAL_GOTO_DISPATCH_OPCODE(EcmaOpcode::LESSEQDYN_PREF_V8);
    }
    HANDLE_OPCODE(DEBUG_HANDLE_GREATERDYN_PREF_V8)
    {
        NOTIFY_DEBUGGER_EVENT();
        REAL_GOTO_DISPATCH_OPCODE(EcmaOpcode::GREATERDYN_PREF_V8);
    }
    HANDLE_OPCODE(DEBUG_HANDLE_GREATEREQDYN_PREF_V8)
    {
        NOTIFY_DEBUGGER_EVENT();
        REAL_GOTO_DISPATCH_OPCODE(EcmaOpcode::GREATEREQDYN_PREF_V8);
    }
    HANDLE_OPCODE(DEBUG_HANDLE_SHL2DYN_PREF_V8)
    {
        NOTIFY_DEBUGGER_EVENT();
        REAL_GOTO_DISPATCH_OPCODE(EcmaOpcode::SHL2DYN_PREF_V8);
    }
    HANDLE_OPCODE(DEBUG_HANDLE_SHR2DYN_PREF_V8)
    {
        NOTIFY_DEBUGGER_EVENT();
        REAL_GOTO_DISPATCH_OPCODE(EcmaOpcode::SHR2DYN_PREF_V8);
    }
    HANDLE_OPCODE(DEBUG_HANDLE_ASHR2DYN_PREF_V8)
    {
        NOTIFY_DEBUGGER_EVENT();
        REAL_GOTO_DISPATCH_OPCODE(EcmaOpcode::ASHR2DYN_PREF_V8);
    }
    HANDLE_OPCODE(DEBUG_HANDLE_AND2DYN_PREF_V8)
    {
        NOTIFY_DEBUGGER_EVENT();
        REAL_GOTO_DISPATCH_OPCODE(EcmaOpcode::AND2DYN_PREF_V8);
    }
    HANDLE_OPCODE(DEBUG_HANDLE_OR2DYN_PREF_V8)
    {
        NOTIFY_DEBUGGER_EVENT();
        REAL_GOTO_DISPATCH_OPCODE(EcmaOpcode::OR2DYN_PREF_V8);
    }
    HANDLE_OPCODE(DEBUG_HANDLE_XOR2DYN_PREF_V8)
    {
        NOTIFY_DEBUGGER_EVENT();
        REAL_GOTO_DISPATCH_OPCODE(EcmaOpcode::XOR2DYN_PREF_V8);
    }
    HANDLE_OPCODE(DEBUG_HANDLE_TONUMBER_PREF_V8)
    {
        NOTIFY_DEBUGGER_EVENT();
        REAL_GOTO_DISPATCH_OPCODE(EcmaOpcode::TONUMBER_PREF_V8);
    }
    HANDLE_OPCODE(DEBUG_HANDLE_NEGDYN_PREF_V8)
    {
        NOTIFY_DEBUGGER_EVENT();
        REAL_GOTO_DISPATCH_OPCODE(EcmaOpcode::NEGDYN_PREF_V8);
    }
    HANDLE_OPCODE(DEBUG_HANDLE_NOTDYN_PREF_V8)
    {
        NOTIFY_DEBUGGER_EVENT();
        REAL_GOTO_DISPATCH_OPCODE(EcmaOpcode::NOTDYN_PREF_V8);
    }
    HANDLE_OPCODE(DEBUG_HANDLE_INCDYN_PREF_V8)
    {
        NOTIFY_DEBUGGER_EVENT();
        REAL_GOTO_DISPATCH_OPCODE(EcmaOpcode::INCDYN_PREF_V8);
    }
    HANDLE_OPCODE(DEBUG_HANDLE_DECDYN_PREF_V8)
    {
        NOTIFY_DEBUGGER_EVENT();
        REAL_GOTO_DISPATCH_OPCODE(EcmaOpcode::DECDYN_PREF_V8);
    }
    HANDLE_OPCODE(DEBUG_HANDLE_EXPDYN_PREF_V8)
    {
        NOTIFY_DEBUGGER_EVENT();
        REAL_GOTO_DISPATCH_OPCODE(EcmaOpcode::EXPDYN_PREF_V8);
    }
    HANDLE_OPCODE(DEBUG_HANDLE_ISINDYN_PREF_V8)
    {
        NOTIFY_DEBUGGER_EVENT();
        REAL_GOTO_DISPATCH_OPCODE(EcmaOpcode::ISINDYN_PREF_V8);
    }
    HANDLE_OPCODE(DEBUG_HANDLE_INSTANCEOFDYN_PREF_V8)
    {
        NOTIFY_DEBUGGER_EVENT();
        REAL_GOTO_DISPATCH_OPCODE(EcmaOpcode::INSTANCEOFDYN_PREF_V8);
    }
    HANDLE_OPCODE(DEBUG_HANDLE_STRICTNOTEQDYN_PREF_V8)
    {
        NOTIFY_DEBUGGER_EVENT();
        REAL_GOTO_DISPATCH_OPCODE(EcmaOpcode::STRICTNOTEQDYN_PREF_V8);
    }
    HANDLE_OPCODE(DEBUG_HANDLE_STRICTEQDYN_PREF_V8)
    {
        NOTIFY_DEBUGGER_EVENT();
        REAL_GOTO_DISPATCH_OPCODE(EcmaOpcode::STRICTEQDYN_PREF_V8);
    }
    HANDLE_OPCODE(DEBUG_HANDLE_RESUMEGENERATOR_PREF_V8)
    {
        NOTIFY_DEBUGGER_EVENT();
        REAL_GOTO_DISPATCH_OPCODE(EcmaOpcode::RESUMEGENERATOR_PREF_V8);
    }
    HANDLE_OPCODE(DEBUG_HANDLE_GETRESUMEMODE_PREF_V8)
    {
        NOTIFY_DEBUGGER_EVENT();
        REAL_GOTO_DISPATCH_OPCODE(EcmaOpcode::GETRESUMEMODE_PREF_V8);
    }
    HANDLE_OPCODE(DEBUG_HANDLE_CREATEGENERATOROBJ_PREF_V8)
    {
        NOTIFY_DEBUGGER_EVENT();
        REAL_GOTO_DISPATCH_OPCODE(EcmaOpcode::CREATEGENERATOROBJ_PREF_V8);
    }
    HANDLE_OPCODE(DEBUG_HANDLE_THROWCONSTASSIGNMENT_PREF_V8)
    {
        NOTIFY_DEBUGGER_EVENT();
        REAL_GOTO_DISPATCH_OPCODE(EcmaOpcode::THROWCONSTASSIGNMENT_PREF_V8);
    }
    HANDLE_OPCODE(DEBUG_HANDLE_GETTEMPLATEOBJECT_PREF_V8)
    {
        NOTIFY_DEBUGGER_EVENT();
        REAL_GOTO_DISPATCH_OPCODE(EcmaOpcode::GETTEMPLATEOBJECT_PREF_V8);
    }
    HANDLE_OPCODE(DEBUG_HANDLE_GETNEXTPROPNAME_PREF_V8)
    {
        NOTIFY_DEBUGGER_EVENT();
        REAL_GOTO_DISPATCH_OPCODE(EcmaOpcode::GETNEXTPROPNAME_PREF_V8);
    }
    HANDLE_OPCODE(DEBUG_HANDLE_CALLARG0DYN_PREF_V8)
    {
        NOTIFY_DEBUGGER_EVENT();
        REAL_GOTO_DISPATCH_OPCODE(EcmaOpcode::CALLARG0DYN_PREF_V8);
    }
    HANDLE_OPCODE(DEBUG_HANDLE_THROWIFNOTOBJECT_PREF_V8)
    {
        NOTIFY_DEBUGGER_EVENT();
        REAL_GOTO_DISPATCH_OPCODE(EcmaOpcode::THROWIFNOTOBJECT_PREF_V8);
    }
    HANDLE_OPCODE(DEBUG_HANDLE_ITERNEXT_PREF_V8)
    {
        NOTIFY_DEBUGGER_EVENT();
        REAL_GOTO_DISPATCH_OPCODE(EcmaOpcode::ITERNEXT_PREF_V8);
    }
    HANDLE_OPCODE(DEBUG_HANDLE_CLOSEITERATOR_PREF_V8)
    {
        NOTIFY_DEBUGGER_EVENT();
        REAL_GOTO_DISPATCH_OPCODE(EcmaOpcode::CLOSEITERATOR_PREF_V8);
    }
    HANDLE_OPCODE(DEBUG_HANDLE_COPYMODULE_PREF_V8)
    {
        NOTIFY_DEBUGGER_EVENT();
        REAL_GOTO_DISPATCH_OPCODE(EcmaOpcode::COPYMODULE_PREF_V8);
    }
    HANDLE_OPCODE(DEBUG_HANDLE_SUPERCALLSPREAD_PREF_V8)
    {
        NOTIFY_DEBUGGER_EVENT();
        REAL_GOTO_DISPATCH_OPCODE(EcmaOpcode::SUPERCALLSPREAD_PREF_V8);
    }
    HANDLE_OPCODE(DEBUG_HANDLE_DELOBJPROP_PREF_V8_V8)
    {
        NOTIFY_DEBUGGER_EVENT();
        REAL_GOTO_DISPATCH_OPCODE(EcmaOpcode::DELOBJPROP_PREF_V8_V8);
    }
    HANDLE_OPCODE(DEBUG_HANDLE_NEWOBJSPREADDYN_PREF_V8_V8)
    {
        NOTIFY_DEBUGGER_EVENT();
        REAL_GOTO_DISPATCH_OPCODE(EcmaOpcode::NEWOBJSPREADDYN_PREF_V8_V8);
    }
    HANDLE_OPCODE(DEBUG_HANDLE_CREATEITERRESULTOBJ_PREF_V8_V8)
    {
        NOTIFY_DEBUGGER_EVENT();
        REAL_GOTO_DISPATCH_OPCODE(EcmaOpcode::CREATEITERRESULTOBJ_PREF_V8_V8);
    }
    HANDLE_OPCODE(DEBUG_HANDLE_SUSPENDGENERATOR_PREF_V8_V8)
    {
        NOTIFY_DEBUGGER_EVENT();
        REAL_GOTO_DISPATCH_OPCODE(EcmaOpcode::SUSPENDGENERATOR_PREF_V8_V8);
    }
    HANDLE_OPCODE(DEBUG_HANDLE_ASYNCFUNCTIONAWAITUNCAUGHT_PREF_V8_V8)
    {
        NOTIFY_DEBUGGER_EVENT();
        REAL_GOTO_DISPATCH_OPCODE(EcmaOpcode::ASYNCFUNCTIONAWAITUNCAUGHT_PREF_V8_V8);
    }
    HANDLE_OPCODE(DEBUG_HANDLE_THROWUNDEFINEDIFHOLE_PREF_V8_V8)
    {
        NOTIFY_DEBUGGER_EVENT();
        REAL_GOTO_DISPATCH_OPCODE(EcmaOpcode::THROWUNDEFINEDIFHOLE_PREF_V8_V8);
    }
    HANDLE_OPCODE(DEBUG_HANDLE_CALLARG1DYN_PREF_V8_V8)
    {
        NOTIFY_DEBUGGER_EVENT();
        REAL_GOTO_DISPATCH_OPCODE(EcmaOpcode::CALLARG1DYN_PREF_V8_V8);
    }
    HANDLE_OPCODE(DEBUG_HANDLE_COPYDATAPROPERTIES_PREF_V8_V8)
    {
        NOTIFY_DEBUGGER_EVENT();
        REAL_GOTO_DISPATCH_OPCODE(EcmaOpcode::COPYDATAPROPERTIES_PREF_V8_V8);
    }
    HANDLE_OPCODE(DEBUG_HANDLE_STARRAYSPREAD_PREF_V8_V8)
    {
        NOTIFY_DEBUGGER_EVENT();
        REAL_GOTO_DISPATCH_OPCODE(EcmaOpcode::STARRAYSPREAD_PREF_V8_V8);
    }
    HANDLE_OPCODE(DEBUG_HANDLE_GETITERATORNEXT_PREF_V8_V8)
    {
        NOTIFY_DEBUGGER_EVENT();
        REAL_GOTO_DISPATCH_OPCODE(EcmaOpcode::GETITERATORNEXT_PREF_V8_V8);
    }
    HANDLE_OPCODE(DEBUG_HANDLE_SETOBJECTWITHPROTO_PREF_V8_V8)
    {
        NOTIFY_DEBUGGER_EVENT();
        REAL_GOTO_DISPATCH_OPCODE(EcmaOpcode::SETOBJECTWITHPROTO_PREF_V8_V8);
    }
    HANDLE_OPCODE(DEBUG_HANDLE_LDOBJBYVALUE_PREF_V8_V8)
    {
        NOTIFY_DEBUGGER_EVENT();
        REAL_GOTO_DISPATCH_OPCODE(EcmaOpcode::LDOBJBYVALUE_PREF_V8_V8);
    }
    HANDLE_OPCODE(DEBUG_HANDLE_STOBJBYVALUE_PREF_V8_V8)
    {
        NOTIFY_DEBUGGER_EVENT();
        REAL_GOTO_DISPATCH_OPCODE(EcmaOpcode::STOBJBYVALUE_PREF_V8_V8);
    }
    HANDLE_OPCODE(DEBUG_HANDLE_STOWNBYVALUE_PREF_V8_V8)
    {
        NOTIFY_DEBUGGER_EVENT();
        REAL_GOTO_DISPATCH_OPCODE(EcmaOpcode::STOWNBYVALUE_PREF_V8_V8);
    }
    HANDLE_OPCODE(DEBUG_HANDLE_LDSUPERBYVALUE_PREF_V8_V8)
    {
        NOTIFY_DEBUGGER_EVENT();
        REAL_GOTO_DISPATCH_OPCODE(EcmaOpcode::LDSUPERBYVALUE_PREF_V8_V8);
    }
    HANDLE_OPCODE(DEBUG_HANDLE_STSUPERBYVALUE_PREF_V8_V8)
    {
        NOTIFY_DEBUGGER_EVENT();
        REAL_GOTO_DISPATCH_OPCODE(EcmaOpcode::STSUPERBYVALUE_PREF_V8_V8);
    }
    HANDLE_OPCODE(DEBUG_HANDLE_LDOBJBYINDEX_PREF_V8_IMM32)
    {
        NOTIFY_DEBUGGER_EVENT();
        REAL_GOTO_DISPATCH_OPCODE(EcmaOpcode::LDOBJBYINDEX_PREF_V8_IMM32);
    }
    HANDLE_OPCODE(DEBUG_HANDLE_STOBJBYINDEX_PREF_V8_IMM32)
    {
        NOTIFY_DEBUGGER_EVENT();
        REAL_GOTO_DISPATCH_OPCODE(EcmaOpcode::STOBJBYINDEX_PREF_V8_IMM32);
    }
    HANDLE_OPCODE(DEBUG_HANDLE_STOWNBYINDEX_PREF_V8_IMM32)
    {
        NOTIFY_DEBUGGER_EVENT();
        REAL_GOTO_DISPATCH_OPCODE(EcmaOpcode::STOWNBYINDEX_PREF_V8_IMM32);
    }
    HANDLE_OPCODE(DEBUG_HANDLE_CALLSPREADDYN_PREF_V8_V8_V8)
    {
        NOTIFY_DEBUGGER_EVENT();
        REAL_GOTO_DISPATCH_OPCODE(EcmaOpcode::CALLSPREADDYN_PREF_V8_V8_V8);
    }
    HANDLE_OPCODE(DEBUG_HANDLE_ASYNCFUNCTIONRESOLVE_PREF_V8_V8_V8)
    {
        NOTIFY_DEBUGGER_EVENT();
        REAL_GOTO_DISPATCH_OPCODE(EcmaOpcode::ASYNCFUNCTIONRESOLVE_PREF_V8_V8_V8);
    }
    HANDLE_OPCODE(DEBUG_HANDLE_ASYNCFUNCTIONREJECT_PREF_V8_V8_V8)
    {
        NOTIFY_DEBUGGER_EVENT();
        REAL_GOTO_DISPATCH_OPCODE(EcmaOpcode::ASYNCFUNCTIONREJECT_PREF_V8_V8_V8);
    }
    HANDLE_OPCODE(DEBUG_HANDLE_CALLARGS2DYN_PREF_V8_V8_V8)
    {
        NOTIFY_DEBUGGER_EVENT();
        REAL_GOTO_DISPATCH_OPCODE(EcmaOpcode::CALLARGS2DYN_PREF_V8_V8_V8);
    }
    HANDLE_OPCODE(DEBUG_HANDLE_CALLARGS3DYN_PREF_V8_V8_V8_V8)
    {
        NOTIFY_DEBUGGER_EVENT();
        REAL_GOTO_DISPATCH_OPCODE(EcmaOpcode::CALLARGS3DYN_PREF_V8_V8_V8_V8);
    }
    HANDLE_OPCODE(DEBUG_HANDLE_DEFINEGETTERSETTERBYVALUE_PREF_V8_V8_V8_V8)
    {
        NOTIFY_DEBUGGER_EVENT();
        REAL_GOTO_DISPATCH_OPCODE(EcmaOpcode::DEFINEGETTERSETTERBYVALUE_PREF_V8_V8_V8_V8);
    }
    HANDLE_OPCODE(DEBUG_HANDLE_NEWOBJDYNRANGE_PREF_IMM16_V8)
    {
        NOTIFY_DEBUGGER_EVENT();
        REAL_GOTO_DISPATCH_OPCODE(EcmaOpcode::NEWOBJDYNRANGE_PREF_IMM16_V8);
    }
    HANDLE_OPCODE(DEBUG_HANDLE_CALLIRANGEDYN_PREF_IMM16_V8)
    {
        NOTIFY_DEBUGGER_EVENT();
        REAL_GOTO_DISPATCH_OPCODE(EcmaOpcode::CALLIRANGEDYN_PREF_IMM16_V8);
    }
    HANDLE_OPCODE(DEBUG_HANDLE_CALLITHISRANGEDYN_PREF_IMM16_V8)
    {
        NOTIFY_DEBUGGER_EVENT();
        REAL_GOTO_DISPATCH_OPCODE(EcmaOpcode::CALLITHISRANGEDYN_PREF_IMM16_V8);
    }
    HANDLE_OPCODE(DEBUG_HANDLE_SUPERCALL_PREF_IMM16_V8)
    {
        NOTIFY_DEBUGGER_EVENT();
        REAL_GOTO_DISPATCH_OPCODE(EcmaOpcode::SUPERCALL_PREF_IMM16_V8);
    }
    HANDLE_OPCODE(DEBUG_HANDLE_CREATEOBJECTWITHEXCLUDEDKEYS_PREF_IMM16_V8_V8)
    {
        NOTIFY_DEBUGGER_EVENT();
        REAL_GOTO_DISPATCH_OPCODE(EcmaOpcode::CREATEOBJECTWITHEXCLUDEDKEYS_PREF_IMM16_V8_V8);
    }
    HANDLE_OPCODE(DEBUG_HANDLE_DEFINEFUNCDYN_PREF_ID16_IMM16_V8)
    {
        NOTIFY_DEBUGGER_EVENT();
        REAL_GOTO_DISPATCH_OPCODE(EcmaOpcode::DEFINEFUNCDYN_PREF_ID16_IMM16_V8);
    }
    HANDLE_OPCODE(DEBUG_HANDLE_DEFINENCFUNCDYN_PREF_ID16_IMM16_V8)
    {
        NOTIFY_DEBUGGER_EVENT();
        REAL_GOTO_DISPATCH_OPCODE(EcmaOpcode::DEFINENCFUNCDYN_PREF_ID16_IMM16_V8);
    }
    HANDLE_OPCODE(DEBUG_HANDLE_DEFINEGENERATORFUNC_PREF_ID16_IMM16_V8)
    {
        NOTIFY_DEBUGGER_EVENT();
        REAL_GOTO_DISPATCH_OPCODE(EcmaOpcode::DEFINEGENERATORFUNC_PREF_ID16_IMM16_V8);
    }
    HANDLE_OPCODE(DEBUG_HANDLE_DEFINEASYNCGENERATORFUNC_PREF_ID16_IMM16_V8)
    {
        NOTIFY_DEBUGGER_EVENT();
        REAL_GOTO_DISPATCH_OPCODE(EcmaOpcode::DEFINEASYNCGENERATORFUNC_PREF_ID16_IMM16_V8);
    }
    HANDLE_OPCODE(DEBUG_HANDLE_DEFINEASYNCFUNC_PREF_ID16_IMM16_V8)
    {
        NOTIFY_DEBUGGER_EVENT();
        REAL_GOTO_DISPATCH_OPCODE(EcmaOpcode::DEFINEASYNCFUNC_PREF_ID16_IMM16_V8);
    }
    HANDLE_OPCODE(DEBUG_HANDLE_DEFINEMETHOD_PREF_ID16_IMM16_V8)
    {
        NOTIFY_DEBUGGER_EVENT();
        REAL_GOTO_DISPATCH_OPCODE(EcmaOpcode::DEFINEMETHOD_PREF_ID16_IMM16_V8);
    }
    HANDLE_OPCODE(DEBUG_HANDLE_NEWLEXENVDYN_PREF_IMM16)
    {
        NOTIFY_DEBUGGER_EVENT();
        REAL_GOTO_DISPATCH_OPCODE(EcmaOpcode::NEWLEXENVDYN_PREF_IMM16);
    }
    HANDLE_OPCODE(DEBUG_HANDLE_COPYRESTARGS_PREF_IMM16)
    {
        NOTIFY_DEBUGGER_EVENT();
        REAL_GOTO_DISPATCH_OPCODE(EcmaOpcode::COPYRESTARGS_PREF_IMM16);
    }
    HANDLE_OPCODE(DEBUG_HANDLE_CREATEARRAYWITHBUFFER_PREF_IMM16)
    {
        NOTIFY_DEBUGGER_EVENT();
        REAL_GOTO_DISPATCH_OPCODE(EcmaOpcode::CREATEARRAYWITHBUFFER_PREF_IMM16);
    }
    HANDLE_OPCODE(DEBUG_HANDLE_CREATEOBJECTHAVINGMETHOD_PREF_IMM16)
    {
        NOTIFY_DEBUGGER_EVENT();
        REAL_GOTO_DISPATCH_OPCODE(EcmaOpcode::CREATEOBJECTHAVINGMETHOD_PREF_IMM16);
    }
    HANDLE_OPCODE(DEBUG_HANDLE_THROWIFSUPERNOTCORRECTCALL_PREF_IMM16)
    {
        NOTIFY_DEBUGGER_EVENT();
        REAL_GOTO_DISPATCH_OPCODE(EcmaOpcode::THROWIFSUPERNOTCORRECTCALL_PREF_IMM16);
    }
    HANDLE_OPCODE(DEBUG_HANDLE_CREATEOBJECTWITHBUFFER_PREF_IMM16)
    {
        NOTIFY_DEBUGGER_EVENT();
        REAL_GOTO_DISPATCH_OPCODE(EcmaOpcode::CREATEOBJECTWITHBUFFER_PREF_IMM16);
    }
    HANDLE_OPCODE(DEBUG_HANDLE_LDLEXVARDYN_PREF_IMM4_IMM4)
    {
        NOTIFY_DEBUGGER_EVENT();
        REAL_GOTO_DISPATCH_OPCODE(EcmaOpcode::LDLEXVARDYN_PREF_IMM4_IMM4);
    }
    HANDLE_OPCODE(DEBUG_HANDLE_LDLEXVARDYN_PREF_IMM8_IMM8)
    {
        NOTIFY_DEBUGGER_EVENT();
        REAL_GOTO_DISPATCH_OPCODE(EcmaOpcode::LDLEXVARDYN_PREF_IMM8_IMM8);
    }
    HANDLE_OPCODE(DEBUG_HANDLE_LDLEXVARDYN_PREF_IMM16_IMM16)
    {
        NOTIFY_DEBUGGER_EVENT();
        REAL_GOTO_DISPATCH_OPCODE(EcmaOpcode::LDLEXVARDYN_PREF_IMM16_IMM16);
    }
    HANDLE_OPCODE(DEBUG_HANDLE_STLEXVARDYN_PREF_IMM4_IMM4_V8)
    {
        NOTIFY_DEBUGGER_EVENT();
        REAL_GOTO_DISPATCH_OPCODE(EcmaOpcode::STLEXVARDYN_PREF_IMM4_IMM4_V8);
    }
    HANDLE_OPCODE(DEBUG_HANDLE_STLEXVARDYN_PREF_IMM8_IMM8_V8)
    {
        NOTIFY_DEBUGGER_EVENT();
        REAL_GOTO_DISPATCH_OPCODE(EcmaOpcode::STLEXVARDYN_PREF_IMM8_IMM8_V8);
    }
    HANDLE_OPCODE(DEBUG_HANDLE_STLEXVARDYN_PREF_IMM16_IMM16_V8)
    {
        NOTIFY_DEBUGGER_EVENT();
        REAL_GOTO_DISPATCH_OPCODE(EcmaOpcode::STLEXVARDYN_PREF_IMM16_IMM16_V8);
    }
    HANDLE_OPCODE(DEBUG_HANDLE_NEWLEXENVWITHNAMEDYN_PREF_IMM16_IMM16)
    {
        NOTIFY_DEBUGGER_EVENT();
        REAL_GOTO_DISPATCH_OPCODE(EcmaOpcode::NEWLEXENVWITHNAMEDYN_PREF_IMM16_IMM16);
    }
    HANDLE_OPCODE(DEBUG_HANDLE_DEFINECLASSWITHBUFFER_PREF_ID16_IMM16_IMM16_V8_V8)
    {
        NOTIFY_DEBUGGER_EVENT();
        REAL_GOTO_DISPATCH_OPCODE(EcmaOpcode::DEFINECLASSWITHBUFFER_PREF_ID16_IMM16_IMM16_V8_V8);
    }
    HANDLE_OPCODE(DEBUG_HANDLE_GETMODULENAMESPACE_PREF_ID32)
    {
        NOTIFY_DEBUGGER_EVENT();
        REAL_GOTO_DISPATCH_OPCODE(EcmaOpcode::GETMODULENAMESPACE_PREF_ID32);
    }
    HANDLE_OPCODE(DEBUG_HANDLE_STMODULEVAR_PREF_ID32)
    {
        NOTIFY_DEBUGGER_EVENT();
        REAL_GOTO_DISPATCH_OPCODE(EcmaOpcode::STMODULEVAR_PREF_ID32);
    }
    HANDLE_OPCODE(DEBUG_HANDLE_TRYLDGLOBALBYNAME_PREF_ID32)
    {
        NOTIFY_DEBUGGER_EVENT();
        REAL_GOTO_DISPATCH_OPCODE(EcmaOpcode::TRYLDGLOBALBYNAME_PREF_ID32);
    }
    HANDLE_OPCODE(DEBUG_HANDLE_TRYSTGLOBALBYNAME_PREF_ID32)
    {
        NOTIFY_DEBUGGER_EVENT();
        REAL_GOTO_DISPATCH_OPCODE(EcmaOpcode::TRYSTGLOBALBYNAME_PREF_ID32);
    }
    HANDLE_OPCODE(DEBUG_HANDLE_LDGLOBALVAR_PREF_ID32)
    {
        NOTIFY_DEBUGGER_EVENT();
        REAL_GOTO_DISPATCH_OPCODE(EcmaOpcode::LDGLOBALVAR_PREF_ID32);
    }
    HANDLE_OPCODE(DEBUG_HANDLE_STGLOBALVAR_PREF_ID32)
    {
        NOTIFY_DEBUGGER_EVENT();
        REAL_GOTO_DISPATCH_OPCODE(EcmaOpcode::STGLOBALVAR_PREF_ID32);
    }
    HANDLE_OPCODE(DEBUG_HANDLE_LDOBJBYNAME_PREF_ID32_V8)
    {
        NOTIFY_DEBUGGER_EVENT();
        REAL_GOTO_DISPATCH_OPCODE(EcmaOpcode::LDOBJBYNAME_PREF_ID32_V8);
    }
    HANDLE_OPCODE(DEBUG_HANDLE_STOBJBYNAME_PREF_ID32_V8)
    {
        NOTIFY_DEBUGGER_EVENT();
        REAL_GOTO_DISPATCH_OPCODE(EcmaOpcode::STOBJBYNAME_PREF_ID32_V8);
    }
    HANDLE_OPCODE(DEBUG_HANDLE_STOWNBYNAME_PREF_ID32_V8)
    {
        NOTIFY_DEBUGGER_EVENT();
        REAL_GOTO_DISPATCH_OPCODE(EcmaOpcode::STOWNBYNAME_PREF_ID32_V8);
    }
    HANDLE_OPCODE(DEBUG_HANDLE_LDSUPERBYNAME_PREF_ID32_V8)
    {
        NOTIFY_DEBUGGER_EVENT();
        REAL_GOTO_DISPATCH_OPCODE(EcmaOpcode::LDSUPERBYNAME_PREF_ID32_V8);
    }
    HANDLE_OPCODE(DEBUG_HANDLE_STSUPERBYNAME_PREF_ID32_V8)
    {
        NOTIFY_DEBUGGER_EVENT();
        REAL_GOTO_DISPATCH_OPCODE(EcmaOpcode::STSUPERBYNAME_PREF_ID32_V8);
    }
    HANDLE_OPCODE(DEBUG_HANDLE_LDMODULEVAR_PREF_ID32_IMM8)
    {
        NOTIFY_DEBUGGER_EVENT();
        REAL_GOTO_DISPATCH_OPCODE(EcmaOpcode::LDMODULEVAR_PREF_ID32_IMM8);
    }
    HANDLE_OPCODE(DEBUG_HANDLE_CREATEREGEXPWITHLITERAL_PREF_ID32_IMM8)
    {
        NOTIFY_DEBUGGER_EVENT();
        REAL_GOTO_DISPATCH_OPCODE(EcmaOpcode::CREATEREGEXPWITHLITERAL_PREF_ID32_IMM8);
    }
    HANDLE_OPCODE(DEBUG_HANDLE_ISTRUE_PREF)
    {
        NOTIFY_DEBUGGER_EVENT();
        REAL_GOTO_DISPATCH_OPCODE(EcmaOpcode::ISTRUE_PREF);
    }
    HANDLE_OPCODE(DEBUG_HANDLE_ISFALSE_PREF)
    {
        NOTIFY_DEBUGGER_EVENT();
        REAL_GOTO_DISPATCH_OPCODE(EcmaOpcode::ISFALSE_PREF);
    }
    HANDLE_OPCODE(DEBUG_HANDLE_STCONSTTOGLOBALRECORD_PREF_ID32)
    {
        NOTIFY_DEBUGGER_EVENT();
        REAL_GOTO_DISPATCH_OPCODE(EcmaOpcode::STCONSTTOGLOBALRECORD_PREF_ID32);
    }
    HANDLE_OPCODE(DEBUG_HANDLE_STLETTOGLOBALRECORD_PREF_ID32)
    {
        NOTIFY_DEBUGGER_EVENT();
        REAL_GOTO_DISPATCH_OPCODE(EcmaOpcode::STLETTOGLOBALRECORD_PREF_ID32);
    }
    HANDLE_OPCODE(DEBUG_HANDLE_STCLASSTOGLOBALRECORD_PREF_ID32)
    {
        NOTIFY_DEBUGGER_EVENT();
        REAL_GOTO_DISPATCH_OPCODE(EcmaOpcode::STCLASSTOGLOBALRECORD_PREF_ID32);
    }
    HANDLE_OPCODE(DEBUG_HANDLE_STOWNBYVALUEWITHNAMESET_PREF_V8_V8)
    {
        NOTIFY_DEBUGGER_EVENT();
        REAL_GOTO_DISPATCH_OPCODE(EcmaOpcode::STOWNBYVALUEWITHNAMESET_PREF_V8_V8);
    }
    HANDLE_OPCODE(DEBUG_HANDLE_STOWNBYNAMEWITHNAMESET_PREF_ID32_V8)
    {
        NOTIFY_DEBUGGER_EVENT();
        REAL_GOTO_DISPATCH_OPCODE(EcmaOpcode::STOWNBYNAMEWITHNAMESET_PREF_ID32_V8);
    }
    HANDLE_OPCODE(DEBUG_HANDLE_LDFUNCTION_PREF)
    {
        NOTIFY_DEBUGGER_EVENT();
        REAL_GOTO_DISPATCH_OPCODE(EcmaOpcode::LDFUNCTION_PREF);
    }
    HANDLE_OPCODE(DEBUG_HANDLE_LDBIGINT_PREF_ID32)
    {
        NOTIFY_DEBUGGER_EVENT();
        REAL_GOTO_DISPATCH_OPCODE(EcmaOpcode::LDBIGINT_PREF_ID32);
    }
    HANDLE_OPCODE(DEBUG_HANDLE_TONUMERIC_PREF_V8)
    {
        NOTIFY_DEBUGGER_EVENT();
        REAL_GOTO_DISPATCH_OPCODE(EcmaOpcode::TONUMERIC_PREF_V8);
    }
    HANDLE_OPCODE(DEBUG_HANDLE_CREATEASYNCGENERATOROBJ_PREF_V8)
    {
        NOTIFY_DEBUGGER_EVENT();
        REAL_GOTO_DISPATCH_OPCODE(EcmaOpcode::CREATEASYNCGENERATOROBJ_PREF_V8);
    }
    HANDLE_OPCODE(DEBUG_HANDLE_ASYNCGENERATORRESOLVE_PREF_V8_V8_V8)
    {
        NOTIFY_DEBUGGER_EVENT();
        REAL_GOTO_DISPATCH_OPCODE(EcmaOpcode::ASYNCGENERATORRESOLVE_PREF_V8_V8_V8);
    }
    HANDLE_OPCODE(DEBUG_HANDLE_DYNAMICIMPORT_PREF_V8)
    {
        NOTIFY_DEBUGGER_EVENT();
        REAL_GOTO_DISPATCH_OPCODE(EcmaOpcode::DYNAMICIMPORT_PREF_V8);
    }
    HANDLE_OPCODE(DEBUG_HANDLE_MOV_DYN_V8_V8)
    {
        NOTIFY_DEBUGGER_EVENT();
        REAL_GOTO_DISPATCH_OPCODE(EcmaOpcode::MOV_DYN_V8_V8);
    }
    HANDLE_OPCODE(DEBUG_HANDLE_MOV_DYN_V16_V16)
    {
        NOTIFY_DEBUGGER_EVENT();
        REAL_GOTO_DISPATCH_OPCODE(EcmaOpcode::MOV_DYN_V16_V16);
    }
    HANDLE_OPCODE(DEBUG_HANDLE_LDA_STR_ID32)
    {
        NOTIFY_DEBUGGER_EVENT();
        REAL_GOTO_DISPATCH_OPCODE(EcmaOpcode::LDA_STR_ID32);
    }
    HANDLE_OPCODE(DEBUG_HANDLE_LDAI_DYN_IMM32)
    {
        NOTIFY_DEBUGGER_EVENT();
        REAL_GOTO_DISPATCH_OPCODE(EcmaOpcode::LDAI_DYN_IMM32);
    }
    HANDLE_OPCODE(DEBUG_HANDLE_FLDAI_DYN_IMM64)
    {
        NOTIFY_DEBUGGER_EVENT();
        REAL_GOTO_DISPATCH_OPCODE(EcmaOpcode::FLDAI_DYN_IMM64);
    }
    HANDLE_OPCODE(DEBUG_HANDLE_JMP_IMM8)
    {
        NOTIFY_DEBUGGER_EVENT();
        REAL_GOTO_DISPATCH_OPCODE(EcmaOpcode::JMP_IMM8);
    }
    HANDLE_OPCODE(DEBUG_HANDLE_JMP_IMM16)
    {
        NOTIFY_DEBUGGER_EVENT();
        REAL_GOTO_DISPATCH_OPCODE(EcmaOpcode::JMP_IMM16);
    }
    HANDLE_OPCODE(DEBUG_HANDLE_JMP_IMM32)
    {
        NOTIFY_DEBUGGER_EVENT();
        REAL_GOTO_DISPATCH_OPCODE(EcmaOpcode::JMP_IMM32);
    }
    HANDLE_OPCODE(DEBUG_HANDLE_JEQZ_IMM8)
    {
        NOTIFY_DEBUGGER_EVENT();
        REAL_GOTO_DISPATCH_OPCODE(EcmaOpcode::JEQZ_IMM8);
    }
    HANDLE_OPCODE(DEBUG_HANDLE_JEQZ_IMM16)
    {
        NOTIFY_DEBUGGER_EVENT();
        REAL_GOTO_DISPATCH_OPCODE(EcmaOpcode::JEQZ_IMM16);
    }
    HANDLE_OPCODE(DEBUG_HANDLE_LDA_DYN_V8)
    {
        NOTIFY_DEBUGGER_EVENT();
        REAL_GOTO_DISPATCH_OPCODE(EcmaOpcode::LDA_DYN_V8);
    }
    HANDLE_OPCODE(DEBUG_HANDLE_STA_DYN_V8)
    {
        NOTIFY_DEBUGGER_EVENT();
        REAL_GOTO_DISPATCH_OPCODE(EcmaOpcode::STA_DYN_V8);
    }
    HANDLE_OPCODE(DEBUG_HANDLE_RETURN_DYN)
    {
        NOTIFY_DEBUGGER_EVENT();
        REAL_GOTO_DISPATCH_OPCODE(EcmaOpcode::RETURN_DYN);
    }
    HANDLE_OPCODE(DEBUG_HANDLE_MOV_V4_V4)
    {
        NOTIFY_DEBUGGER_EVENT();
        REAL_GOTO_DISPATCH_OPCODE(EcmaOpcode::MOV_V4_V4);
    }
    HANDLE_OPCODE(DEBUG_HANDLE_JNEZ_IMM8)
    {
        NOTIFY_DEBUGGER_EVENT();
        REAL_GOTO_DISPATCH_OPCODE(EcmaOpcode::JNEZ_IMM8);
    }
    HANDLE_OPCODE(DEBUG_HANDLE_JNEZ_IMM16)
    {
        NOTIFY_DEBUGGER_EVENT();
        REAL_GOTO_DISPATCH_OPCODE(EcmaOpcode::JNEZ_IMM16);
    }
    HANDLE_OPCODE(DEBUG_EXCEPTION_HANDLER)
    {
        NOTIFY_DEBUGGER_EXCEPTION_EVENT();
        REAL_GOTO_EXCEPTION_HANDLER();
    }
    HANDLE_OPCODE(DEBUG_HANDLE_OVERFLOW)
    {
        REAL_GOTO_DISPATCH_OPCODE(EcmaOpcode::LAST_OPCODE + 1);
    }
