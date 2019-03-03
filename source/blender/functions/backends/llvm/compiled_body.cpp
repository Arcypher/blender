#include "FN_llvm.hpp"
#include "ir_utils.hpp"

#include <llvm/IR/Verifier.h>
#include <llvm/ExecutionEngine/ExecutionEngine.h>

namespace FN {

	const char *CompiledLLVMBody::identifier_in_composition()
	{
		return "Compiled LLVM Body";
	}

	void CompiledLLVMBody::free_self(void *value)
	{
		CompiledLLVMBody *v = (CompiledLLVMBody *)value;
		delete v;
	}

	CompiledLLVMBody::~CompiledLLVMBody()
	{
		delete m_engine;
	}

	CompiledLLVMBody *CompiledLLVMBody::FromIR(
		llvm::Module *module,
		llvm::Function *main_func)
	{
		BLI_assert(!llvm::verifyModule(*module, &llvm::outs()));

		llvm::ExecutionEngine *ee = llvm::EngineBuilder(
			std::unique_ptr<llvm::Module>(module)).create();
		ee->finalizeObject();
		ee->generateCodeForModule(module);

		module->print(llvm::outs(), nullptr);

		uint64_t function_ptr = ee->getFunctionAddress(
			main_func->getName().str());

		auto body = new CompiledLLVMBody();
		body->m_engine = ee;
		body->m_func_ptr = (void *)function_ptr;
		return body;
	}

	static CompiledLLVMBody *compile_llvm_body(
		SharedFunction &fn,
		llvm::LLVMContext &context)
	{
		auto input_type_infos = fn->signature().input_extensions<LLVMTypeInfo>();
		auto output_type_infos = fn->signature().output_extensions<LLVMTypeInfo>();
		LLVMTypes input_types = types_of_type_infos(input_type_infos, context);
		LLVMTypes output_types = types_of_type_infos(output_type_infos, context);

		llvm::Type *output_type = llvm::StructType::get(context, to_array_ref(output_types));

		llvm::FunctionType *function_type = llvm::FunctionType::get(
			output_type, to_array_ref(input_types), false);

		llvm::Module *module = new llvm::Module(fn->name(), context);

		llvm::Function *function = llvm::Function::Create(
			function_type,
			llvm::GlobalValue::LinkageTypes::ExternalLinkage,
			fn->name(),
			module);

		LLVMValues input_values;
		for (llvm::Value &value : function->args()) {
			input_values.append(&value);
		}

		llvm::BasicBlock *bb = llvm::BasicBlock::Create(context, "entry", function);
		llvm::IRBuilder<> builder(bb);

		LLVMGenBody *gen_body = fn->body<LLVMGenBody>();
		BLI_assert(gen_body);

		LLVMValues output_values;
		gen_body->build_ir(builder, input_values, output_values);
		BLI_assert(output_values.size() == output_types.size());

		llvm::Value *return_value = llvm::UndefValue::get(output_type);
		for (uint i = 0; i < output_values.size(); i++) {
			return_value = builder.CreateInsertValue(return_value, output_values[i], i);
		}
		builder.CreateRet(return_value);

		return CompiledLLVMBody::FromIR(
			module, function);
	}

	bool try_ensure_CompiledLLVMBody(
		SharedFunction &fn,
		llvm::LLVMContext &context)
	{
		if (fn->body<CompiledLLVMBody>() != nullptr) {
			return true;
		}

		if (fn->body<LLVMGenBody>() != nullptr) {
			fn->add_body(compile_llvm_body(fn, context));
			return true;
		}

		return false;
	}

} /* namespace FN */