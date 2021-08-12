#include <mod/il2cpp.h>

namespace IL2CPP::Func
{
    /* Just a functions of our Utils */
    void HookFunctions();

    /* Pointers to IL2CPP functions */
	#ifdef UNITY_2019
		int (*init)(const char* domain_name);
		int (*init_utf16)(const IL2Char * domain_name);
    #else
		void (*init)(const char* domain_name);
		void (*init_utf16)(const IL2Char * domain_name);
    #endif
    void (*shutdown)();
    void (*set_config_dir)(const char *config_path);
    void (*set_data_dir)(const char *data_path);
    void (*set_temp_dir)(const char *temp_path);
    void (*set_commandline_arguments)(int argc, const char* const argv[], const char* basedir);
    void (*set_commandline_arguments_utf16)(int argc, const IL2Char * const argv[], const char* basedir);
    void (*set_config_utf16)(const IL2Char * executablePath);
    void (*set_config)(const char* executablePath);
    void (*set_memory_callbacks)(IL2MemoryCallbacks * callbacks);
    const IL2Image* (*get_corlib)();
    void (*add_internal_call)(const char* name, IL2MethodPointer method);
    IL2MethodPointer (*resolve_icall)(const char* name);
    void* (*alloc)(size_t size);
    void (*free)(void* ptr);
    IL2Class* (*array_class_get)(IL2Class * element_class, uint32_t rank);
    uint32_t (*array_length)(IL2Array * array);
    uint32_t (*array_get_byte_length)(IL2Array * array);
    IL2Array* (*array_new)(IL2Class * elementTypeInfo, il2cpp_array_size_t length);
    IL2Array* (*array_new_specific)(IL2Class * arrayTypeInfo, il2cpp_array_size_t length);
    IL2Array* (*array_new_full)(IL2Class * array_class, il2cpp_array_size_t * lengths, il2cpp_array_size_t * lower_bounds);
    IL2Class* (*bounded_array_class_get)(IL2Class * element_class, uint32_t rank, bool bounded);
    int (*array_element_size)(const IL2Class * array_class);
    const IL2Image* (*assembly_get_image)(const IL2Assembly * assembly);
    #ifdef UNITY_2019
		void (*class_for_each)(void(*klassReportFunc)(IL2Class* klass, void* userData), void* userData);
    #endif
    const IL2Type* (*class_enum_basetype)(IL2Class * klass);
    bool (*class_is_generic)(const IL2Class * klass);
    bool (*class_is_inflated)(const IL2Class * klass);
    bool (*class_is_assignable_from)(IL2Class * klass, IL2Class * oklass);
    bool (*class_is_subclass_of)(IL2Class * klass, IL2Class * klassc, bool check_interfaces);
    bool (*class_has_parent)(IL2Class * klass, IL2Class * klassc);
    IL2Class* (*class_from_il2cpp_type)(const IL2Type * type);
    IL2Class* (*class_from_name)(const IL2Image * image, const char* namespaze, const char *name);
    IL2Class* (*class_from_system_type)(Il2CppReflectionType * type);
    IL2Class* (*class_get_element_class)(IL2Class * klass);
    const EventInfo* (*class_get_events)(IL2Class * klass, void* *iter);
    FieldInfo* (*class_get_fields)(IL2Class * klass, void* *iter);
    IL2Class* (*class_get_nested_types)(IL2Class * klass, void* *iter);
    IL2Class* (*class_get_interfaces)(IL2Class * klass, void* *iter);
    const PropertyInfo* (*class_get_properties)(IL2Class * klass, void* *iter);
    const PropertyInfo* (*class_get_property_from_name)(IL2Class * klass, const char *name);
    FieldInfo* (*class_get_field_from_name)(IL2Class * klass, const char *name);
    const MethodInfo* (*class_get_methods)(IL2Class * klass, void* *iter);
    const MethodInfo* (*class_get_method_from_name)(const IL2Class * klass, const char* name, int argsCount);
    const char* (*class_get_name)(const IL2Class * klass);
    #ifdef UNITY_2019
		void (*type_get_name_chunked)(const IL2Type * type, void(*chunkReportFunc)(void* data, void* userData), void* userData);
    #endif
    const char* (*class_get_namespace)(const IL2Class * klass);
    IL2Class* (*class_get_parent)(IL2Class * klass);
    IL2Class* (*class_get_declaring_type)(const IL2Class * klass);
    int32_t (*class_instance_size)(IL2Class * klass);
    size_t (*class_num_fields)(const IL2Class * enumKlass);
    bool (*class_is_valuetype)(const IL2Class * klass);
    int32_t (*class_value_size)(IL2Class * klass, uint32_t * align);
    bool (*class_is_blittable)(const IL2Class * klass);
    int (*class_get_flags)(const IL2Class * klass);
    bool (*class_is_abstract)(const IL2Class * klass);
    bool (*class_is_interface)(const IL2Class * klass);
    int (*class_array_element_size)(const IL2Class * klass);
    IL2Class* (*class_from_type)(const IL2Type * type);
    const IL2Type* (*class_get_type)(IL2Class * klass);
    uint32_t (*class_get_type_token)(IL2Class * klass);
    bool (*class_has_attribute)(IL2Class * klass, IL2Class * attr_class);
    bool (*class_has_references)(IL2Class * klass);
    bool (*class_is_enum)(const IL2Class * klass);
    const IL2Image* (*class_get_image)(IL2Class * klass);
    const char* (*class_get_assemblyname)(const IL2Class * klass);
    int (*class_get_rank)(const IL2Class * klass);
    #ifdef UNITY_2019
		uint32_t (*class_get_data_size)(const IL2Class * klass);
		void* (*class_get_static_field_data)(const IL2Class * klass);
    #endif
    size_t (*class_get_bitmap_size)(const IL2Class * klass);
    void (*class_get_bitmap)(IL2Class * klass, size_t * bitmap);
    bool (*stats_dump_to_file)(const char *path);
    uint64_t (*stats_get_value)(Il2CppStat stat);
    Il2CppDomain* (*domain_get)();
    const IL2Assembly* (*domain_assembly_open)(Il2CppDomain * domain, const char* name);
    const IL2Assembly** (*domain_get_assemblies)(const Il2CppDomain * domain, size_t * size);
    #ifdef UNITY_2019
		void (*raise_exception)(IL2Exception*);
    #endif
    IL2Exception* (*exception_from_name_msg)(const IL2Image * image, const char *name_space, const char *name, const char *msg);
    IL2Exception* (*get_exception_argument_null)(const char *arg);
    void (*format_exception)(const IL2Exception * ex, char* message, int message_size);
    void (*format_stack_trace)(const IL2Exception * ex, char* output, int output_size);
    void (*unhandled_exception)(IL2Exception*);
    int (*field_get_flags)(FieldInfo * field);
    const char* (*field_get_name)(FieldInfo * field);
    IL2Class* (*field_get_parent)(FieldInfo * field);
    size_t (*field_get_offset)(FieldInfo * field);
    const IL2Type* (*field_get_type)(FieldInfo * field);
    void (*field_get_value)(IL2Object * obj, FieldInfo * field, void *value);
    IL2Object* (*field_get_value_object)(FieldInfo * field, IL2Object * obj);
    bool (*field_has_attribute)(FieldInfo * field, IL2Class * attr_class);
    void (*field_set_value)(IL2Object * obj, FieldInfo * field, void *value);
    void (*field_static_get_value)(FieldInfo * field, void *value);
    void (*field_static_set_value)(FieldInfo * field, void *value);
    void (*field_set_value_object)(IL2Object * instance, FieldInfo * field, IL2Object * value);
    #ifdef UNITY_2019
		bool (*field_is_literal)(FieldInfo * field);
    #endif
    void (*gc_collect)(int maxGenerations);
    int32_t (*gc_collect_a_little)();
    void (*gc_disable)();
    void (*gc_enable)();
    bool (*gc_is_disabled)();
    #ifdef UNITY_2019
		int64_t (*gc_get_max_time_slice_ns)();
		void (*gc_set_max_time_slice_ns)(int64_t maxTimeSlice);
		bool (*gc_is_incremental)();
    #endif
    int64_t (*gc_get_used_size)();
    int64_t (*gc_get_heap_size)();
    void (*gc_wbarrier_set_field)(IL2Object * obj, void **targetAddress, void *object);
    #ifdef UNITY_2019
		bool (*gc_has_strict_wbarriers)();
		void (*gc_set_external_allocation_tracker)(void(*func)(void*, size_t, int));
		void (*gc_set_external_wbarrier_tracker)(void(*func)(void**));
		void (*gc_foreach_heap)(void(*func)(void* data, void* userData), void* userData);
		void (*stop_gc_world)();
		void (*start_gc_world)();
    #endif
    uint32_t (*gchandle_new)(IL2Object * obj, bool pinned);
    uint32_t (*gchandle_new_weakref)(IL2Object * obj, bool track_resurrection);
    IL2Object* (*gchandle_get_target)(uint32_t gchandle);
    void (*gchandle_free)(uint32_t gchandle);
    #ifdef UNITY_2019
		void (*gchandle_foreach_get_target)(void(*func)(void* data, void* userData), void* userData);
		uint32_t (*object_header_size)();
		uint32_t (*array_object_header_size)();
		uint32_t (*offset_of_array_length_in_array_object_header)();
		uint32_t (*offset_of_array_bounds_in_array_object_header)();
		uint32_t (*allocation_granularity)();
    #endif
    void* (*unity_liveness_calculation_begin)(IL2Class * filter, int max_object_count, il2cpp_register_object_callback callback, void* userdata, il2cpp_WorldChangedCallback onWorldStarted, il2cpp_WorldChangedCallback onWorldStopped);
    void (*unity_liveness_calculation_end)(void* state);
    void (*unity_liveness_calculation_from_root)(IL2Object * root, void* state);
    void (*unity_liveness_calculation_from_statics)(void* state);
    const IL2Type* (*method_get_return_type)(const MethodInfo * method);
    IL2Class* (*method_get_declaring_type)(const MethodInfo * method);
    const char* (*method_get_name)(const MethodInfo * method);
    const MethodInfo* (*method_get_from_reflection)(const IL2ReflectionMethod * method);
    IL2ReflectionMethod* (*method_get_object)(const MethodInfo * method, IL2Class * refclass);
    bool (*method_is_generic)(const MethodInfo * method);
    bool (*method_is_inflated)(const MethodInfo * method);
    bool (*method_is_instance)(const MethodInfo * method);
    uint32_t (*method_get_param_count)(const MethodInfo * method);
    const IL2Type* (*method_get_param)(const MethodInfo * method, uint32_t index);
    IL2Class* (*method_get_class)(const MethodInfo * method);
    bool (*method_has_attribute)(const MethodInfo * method, IL2Class * attr_class);
    uint32_t (*method_get_flags)(const MethodInfo * method, uint32_t * iflags);
    uint32_t (*method_get_token)(const MethodInfo * method);
    const char* (*method_get_param_name)(const MethodInfo * method, uint32_t index);

    // ONLY IF THE PROFILER EXISTS FOR UNITY_2019
    void (*profiler_install)(IL2Profiler * prof, IL2ProfileFunc shutdown_callback);
    void (*profiler_set_events)(Il2CppProfileFlags events);
    void (*profiler_install_enter_leave)(Il2CppProfileMethodFunc enter, Il2CppProfileMethodFunc fleave);
    void (*profiler_install_allocation)(Il2CppProfileAllocFunc callback);
    void (*profiler_install_gc)(Il2CppProfileGCFunc callback, Il2CppProfileGCResizeFunc heap_resize_callback);
    void (*profiler_install_fileio)(Il2CppProfileFileIOFunc callback);
    void (*profiler_install_thread)(Il2CppProfileThreadFunc start, Il2CppProfileThreadFunc end);

    uint32_t (*property_get_flags)(const PropertyInfo * prop);
    const MethodInfo* (*property_get_get_method)(const PropertyInfo * prop);
    const MethodInfo* (*property_get_set_method)(const PropertyInfo * prop);
    const char* (*property_get_name)(const PropertyInfo * prop);
    IL2Class* (*property_get_parent)(const PropertyInfo * prop);
    IL2Class* (*object_get_class)(IL2Object * obj);
    uint32_t (*object_get_size)(IL2Object * obj);
    const MethodInfo* (*object_get_virtual_method)(IL2Object * obj, const MethodInfo * method);
    IL2Object* (*object_new)(const IL2Class * klass);
    void* (*object_unbox)(IL2Object * obj);
    IL2Object* (*value_box)(IL2Class * klass, void* data);
    void (*monitor_enter)(IL2Object * obj);
    bool (*monitor_try_enter)(IL2Object * obj, uint32_t timeout);
    void (*monitor_exit)(IL2Object * obj);
    void (*monitor_pulse)(IL2Object * obj);
    void (*monitor_pulse_all)(IL2Object * obj);
    void (*monitor_wait)(IL2Object * obj);
    bool (*monitor_try_wait)(IL2Object * obj, uint32_t timeout);
    IL2Object* (*runtime_invoke)(const MethodInfo * method, void *obj, void **params, IL2Exception **exc);
    IL2Object* (*runtime_invoke_convert_args)(const MethodInfo * method, void *obj, IL2Object **params, int paramCount, IL2Exception **exc);
    void (*runtime_class_init)(IL2Class * klass);
    void (*runtime_object_init)(IL2Object * obj);
    void (*runtime_object_init_exception)(IL2Object * obj, IL2Exception** exc);
    void (*runtime_unhandled_exception_policy_set)(IL2RuntimeUnhandledExceptionPolicy value);
    int32_t (*string_length)(IL2String * str);
    IL2Char* (*string_chars)(IL2String * str);
    IL2String* (*string_new)(const char* str);
    IL2String* (*string_new_len)(const char* str, uint32_t length);
    IL2String* (*string_new_utf16)(const IL2Char * text, int32_t len);
    IL2String* (*string_new_wrapper)(const char* str);
    IL2String* (*string_intern)(IL2String * str);
    IL2String* (*string_is_interned)(IL2String * str);
    IL2Thread* (*thread_current)();
    IL2Thread* (*thread_attach)(IL2Domain * domain);
    void (*thread_detach)(IL2Thread * thread);
    IL2Thread** (*thread_get_all_attached_threads)(size_t * size);
    bool (*is_vm_thread)(IL2Thread * thread);
    void (*current_thread_walk_frame_stack)(IL2FrameWalkFunc func, void* user_data);
    void (*thread_walk_frame_stack)(IL2Thread * thread, IL2FrameWalkFunc func, void* user_data);
    bool (*current_thread_get_top_frame)(IL2StackFrameInfo * frame);
    bool (*thread_get_top_frame)(IL2Thread * thread, IL2StackFrameInfo * frame);
    bool (*current_thread_get_frame_at)(int32_t offset, IL2StackFrameInfo * frame);
    bool (*thread_get_frame_at)(IL2Thread * thread, int32_t offset, IL2StackFrameInfo * frame);
    int32_t (*current_thread_get_stack_depth)();
    int32_t (*thread_get_stack_depth)(IL2Thread * thread);
    #ifdef UNITY_2019
		void (*override_stack_backtrace)(IL2BacktraceFunc stackBacktraceFunc);
    #endif
    IL2Object* (*type_get_object)(const IL2Type * type);
    int (*type_get_type)(const IL2Type * type);
    IL2Class* (*type_get_class_or_element_class)(const IL2Type * type);
    char* (*type_get_name)(const IL2Type * type);
    bool (*type_is_byref)(const IL2Type * type);
    uint32_t (*type_get_attrs)(const IL2Type * type);
    bool (*type_equals)(const IL2Type * type, const IL2Type * otherType);
    char* (*type_get_assembly_qualified_name)(const IL2Type * type);
    #ifdef UNITY_2019
		bool (*type_is_static)(const IL2Type * type);
		bool (*type_is_pointer_type)(const IL2Type * type);
    #endif
    const IL2Assembly* (*image_get_assembly)(const IL2Image * image);
    const char* (*image_get_name)(const IL2Image * image);
    const char* (*image_get_filename)(const IL2Image * image);
    const MethodInfo* (*image_get_entry_point)(const IL2Image * image);
    size_t (*image_get_class_count)(const IL2Image * image);
    const IL2Class* (*image_get_class)(const IL2Image * image, size_t index);
    IL2ManagedMemorySnapshot* (*capture_memory_snapshot)();
    void (*free_captured_memory_snapshot)(Il2CppManagedMemorySnapshot * snapshot);
    void (*set_find_plugin_callback)(Il2CppSetFindPlugInCallback method);
    void (*register_log_callback)(Il2CppLogCallback method);
    void (*debugger_set_agent_options)(const char* options);
    bool (*is_debugger_attached)();
    #ifdef UNITY_2019
		void (*register_debugger_agent_transport)(IL2DebuggerTransport * debuggerTransport);
		bool (*debug_get_method_info)(const MethodInfo*, IL2MethodDebugInfo * methodDebugInfo);
    #endif
    void (*unity_install_unitytls_interface)(const void* unitytlsInterfaceStruct);
    IL2CustomAttrInfo* (*custom_attrs_from_class)(IL2Class * klass);
    IL2CustomAttrInfo* (*custom_attrs_from_method)(const MethodInfo * method);
    IL2Object* (*custom_attrs_get_attr)(IL2CustomAttrInfo * ainfo, IL2Class * attr_klass);
    bool (*custom_attrs_has_attr)(IL2CustomAttrInfo * ainfo, IL2Class * attr_klass);
    IL2Array* (*custom_attrs_construct)(IL2CustomAttrInfo * cinfo);
    void (*custom_attrs_free)(IL2CustomAttrInfo * ainfo);
    #ifdef UNITY_2019
		void (*class_set_userdata)(IL2Class * klass, void* userdata);
		int (*class_get_userdata_offset)();
    #endif
}