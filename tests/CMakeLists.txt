add_executable(test_model test_model.c ../src/model.c)
target_include_directories(test_model PRIVATE ../include)
add_test(NAME test_model COMMAND test_model)
