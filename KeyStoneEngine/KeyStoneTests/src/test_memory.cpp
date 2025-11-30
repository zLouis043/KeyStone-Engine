#include <doctest/doctest.h>
#include <keystone.h>

TEST_CASE("Memory Manager Tests") {
	ks_memory_init();

	SUBCASE("Standard Allocation/Deallocation") {
		int* arr = (int*)ks_alloc(10 * sizeof(int), KS_LT_USER_MANAGED, KS_TAG_GARBAGE);
		
		REQUIRE(arr != NULL);

		for (int i = 0; i < 10; i++) {
			arr[i] = i * 10;
		}

		CHECK(arr[0] == 0);
		CHECK(arr[9] == 90);

		ks_dealloc(arr);
	}

    SUBCASE("Reallocation") {
        int* ptr = (int*)ks_alloc(sizeof(int), KS_LT_USER_MANAGED, KS_TAG_GARBAGE);
        REQUIRE(ptr != nullptr);
        *ptr = 123;

        int* new_ptr = (int*)ks_realloc(ptr, 2 * sizeof(int));
        REQUIRE(new_ptr != nullptr);

        CHECK(new_ptr[0] == 123);

        new_ptr[1] = 456;
        CHECK(new_ptr[1] == 456);

        ks_dealloc(new_ptr);
    }

    SUBCASE("Frame Allocator") {
        ks_set_frame_capacity(1024);

        void* f_ptr1 = ks_alloc(100, KS_LT_FRAME, KS_TAG_INTERNAL_DATA);
        void* f_ptr2 = ks_alloc(100, KS_LT_FRAME, KS_TAG_INTERNAL_DATA);

        CHECK(f_ptr1 != nullptr);
        CHECK(f_ptr2 != nullptr);
        CHECK(f_ptr1 != f_ptr2);

        ks_frame_cleanup();

        void* f_ptr3 = ks_alloc(100, KS_LT_FRAME, KS_TAG_INTERNAL_DATA);
        CHECK(f_ptr3 != nullptr);
    }

	ks_memory_shutdown();
}