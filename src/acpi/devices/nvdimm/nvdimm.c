/*
 * Copyright (C) 2017-2020 Canonical
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */
#include "fwts.h"

#if defined(FWTS_HAS_ACPI)

#include "fwts_acpi_object_eval.h"
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <inttypes.h>
#include <string.h>

#define FWTS_ACPI_NVDIMM_HID "ACPI0012"

static ACPI_HANDLE device;

static ACPI_STATUS get_device_handle(ACPI_HANDLE handle, uint32_t level,
					  void *context, void **ret_val)
{
	FWTS_UNUSED(level);
	FWTS_UNUSED(context);
	FWTS_UNUSED(ret_val);

	device = handle;
	return AE_CTRL_TERMINATE;
}

static int acpi_nvdimm_init(fwts_framework *fw)
{
	ACPI_STATUS status;

	if (fwts_acpica_init(fw) != FWTS_OK)
		return FWTS_ERROR;

	status = AcpiGetDevices(FWTS_ACPI_NVDIMM_HID, get_device_handle, NULL, NULL);
	if (ACPI_FAILURE(status)) {
		fwts_log_error(fw, "Cannot find the ACPI device");
		return FWTS_ERROR;
	}

	if (!device) {
		fwts_log_error(fw, "ACPI NVDIMM device does not exist, skipping test");
		fwts_acpica_deinit();
		return FWTS_SKIP;
	} else {
		ACPI_BUFFER buffer;
		char full_name[128];

		buffer.Length = sizeof(full_name);
		buffer.Pointer = full_name;

		status = AcpiGetName(device, ACPI_FULL_PATHNAME, &buffer);
		if (ACPI_SUCCESS(status)) {
			fwts_log_info_verbatim(fw, "ACPI NVDIMM Device: %s", full_name);
			fwts_log_nl(fw);
		}
	}

	return FWTS_OK;
}

static void check_nvdimm_status(
	fwts_framework *fw,
	char *name,
	uint16_t status,
	bool *failed)
{
	if (status > 6) {
		*failed = true;
		fwts_failed(fw, LOG_LEVEL_MEDIUM,
			"MethodBadStatus",
			"%s: Expected Status to be 0..6, got %" PRIx16,
			name, status);
	}
}

static void check_nvdimm_extended_status(
	fwts_framework *fw,
	char *name,
	uint16_t ext_status,
	uint16_t expected,
	bool *failed)
{
	if (ext_status != expected) {
		*failed = true;
		fwts_failed(fw, LOG_LEVEL_MEDIUM,
			"MethodBadExtendedStatus",
			"%s: Expected Extended Status to be %" PRIx16
			", got %" PRIx16, name, expected, ext_status);
	}
}

static void method_test_NCH_return(
	fwts_framework *fw,
	char *name,
	ACPI_BUFFER *buf,
	ACPI_OBJECT *obj,
	void *private)
{
	bool failed = false;
	nch_return_t *ret;

	FWTS_UNUSED(private);

	if (fwts_method_check_type(fw, name, buf, ACPI_TYPE_BUFFER) != FWTS_OK)
		return;

	if (fwts_method_buffer_size(fw, name, obj, 64) != FWTS_OK)
		failed = true;

	ret = (nch_return_t *) obj->Buffer.Pointer;
	check_nvdimm_status(fw, name, ret->status, &failed);
	check_nvdimm_extended_status(fw, name, ret->extended_status, 0, &failed);
	fwts_acpi_reserved_bits_check(fw, "_NCH", "Validation Flags",
		ret->extended_status, sizeof(uint16_t), 2, 15, &failed);

	/* Health Status Flags [2..7], [11.15], [19..31] are reserved */
	fwts_acpi_reserved_bits_check(fw, "_NCH", "Health Status Flags",
		ret->health_status_flags, sizeof(uint32_t), 2, 7, &failed);
	fwts_acpi_reserved_bits_check(fw, "_NCH", "Health Status Flags",
		ret->health_status_flags, sizeof(uint32_t), 11, 15, &failed);
	fwts_acpi_reserved_bits_check(fw, "_NCH", "Health Status Flags",
		ret->health_status_flags, sizeof(uint32_t), 19, 31, &failed);

	fwts_acpi_reserved_bits_check(fw, "_NCH", "Health Status Attributes",
		ret->health_status_attributes, sizeof(uint32_t), 1, 31, &failed);

	if (!failed)
		fwts_method_passed_sane(fw, name, "buffer");
}

static int method_test_NCH(fwts_framework *fw)
{
	return fwts_evaluate_method(fw, METHOD_MANDATORY, &device,
		"_NCH", NULL, 0, method_test_NCH_return, NULL);
}

static void method_test_NBS_return(
	fwts_framework *fw,
	char *name,
	ACPI_BUFFER *buf,
	ACPI_OBJECT *obj,
	void *private)
{
	bool failed = false;
	nbs_return_t *ret;

	FWTS_UNUSED(private);

	if (fwts_method_check_type(fw, name, buf, ACPI_TYPE_BUFFER) != FWTS_OK)
		return;

	if (fwts_method_buffer_size(fw, name, obj, 64) != FWTS_OK)
		failed = true;

	ret = (nbs_return_t *) obj->Buffer.Pointer;
	check_nvdimm_status(fw, name, ret->status, &failed);
	check_nvdimm_extended_status(fw, name, ret->extended_status, 0, &failed);
	fwts_acpi_reserved_bits_check(fw, "_NBS", "Validation Flags",
		ret->validation_flags, sizeof(uint16_t), 1, 15, &failed);

	if (!failed)
		fwts_method_passed_sane(fw, name, "buffer");
}

static int method_test_NBS(fwts_framework *fw)
{
	return fwts_evaluate_method(fw, METHOD_MANDATORY, &device,
		"_NBS", NULL, 0, method_test_NBS_return, NULL);
}

static void method_test_NIC_return(
	fwts_framework *fw,
	char *name,
	ACPI_BUFFER *buf,
	ACPI_OBJECT *obj,
	void *private)
{
	bool failed = false;
	nic_return_t *ret;

	FWTS_UNUSED(private);

	if (fwts_method_check_type(fw, name, buf, ACPI_TYPE_BUFFER) != FWTS_OK)
		return;

	if (fwts_method_buffer_size(fw, name, obj, 64) != FWTS_OK)
		failed = true;

	ret = (nic_return_t *) obj->Buffer.Pointer;
	check_nvdimm_status(fw, name, ret->status, &failed);
	check_nvdimm_extended_status(fw, name, ret->extended_status, 0, &failed);

	/* Health Error Injection Capabilities [2..7], [11.15], [19..31] are reserved */
	fwts_acpi_reserved_bits_check(fw, "_NIC", "Health Error Injection Capabilities",
		ret->health_error_injection, sizeof(uint32_t), 2, 7, &failed);
	fwts_acpi_reserved_bits_check(fw, "_NIC", "Health Error Injection Capabilities",
		ret->health_error_injection, sizeof(uint32_t), 11, 15, &failed);
	fwts_acpi_reserved_bits_check(fw, "_NIC", "Health Error Injection Capabilities",
		ret->health_error_injection, sizeof(uint32_t), 19, 31, &failed);

	fwts_acpi_reserved_bits_check(fw, "_NIC", "Health Status Attributes Capabilities",
		ret->health_status_attributes, sizeof(uint32_t), 1, 31, &failed);

	if (!failed)
		fwts_method_passed_sane(fw, name, "buffer");
}

static int method_test_NIC(fwts_framework *fw)
{
	return fwts_evaluate_method(fw, METHOD_MANDATORY, &device,
		"_NIC", NULL, 0, method_test_NIC_return, NULL);
}


static void method_test_NIH_return(
	fwts_framework *fw,
	char *name,
	ACPI_BUFFER *buf,
	ACPI_OBJECT *obj,
	void *private)
{
	bool failed = false;
	nih_return_t *ret;

	FWTS_UNUSED(private);

	if (fwts_method_check_type(fw, name, buf, ACPI_TYPE_BUFFER) != FWTS_OK)
		return;

	if (fwts_method_buffer_size(fw, name, obj, 64) != FWTS_OK)
		failed = true;

	ret = (nih_return_t *) obj->Buffer.Pointer;
	check_nvdimm_status(fw, name, ret->status, &failed);
	check_nvdimm_extended_status(fw, name, ret->extended_status, 1, &failed);

	if (!failed)
		fwts_method_passed_sane(fw, name, "buffer");
}

static int method_test_NIH(fwts_framework *fw)
{
	return fwts_evaluate_method(fw, METHOD_MANDATORY, &device,
		"_NIH", NULL, 0, method_test_NIH_return, NULL);
}

/* Evaluate Device Identification Objects - all are optional */
static int method_test_HID(fwts_framework *fw)
{
	return fwts_method_test_HID(fw, &device);
}

static fwts_framework_minor_test acpi_nvdimm_tests[] = {
	/* Device Specific Objects */
	{ method_test_NCH, "Test _NCH (NVDIMM Current Health Information)." },
	{ method_test_NBS, "Test _NBS (NVDIMM Boot Status)." },
	{ method_test_NIC, "Test _NIC (NVDIMM Health Error Injection Capabilities)." },
	{ method_test_NIH, "Test _NIH (NVDIMM Inject/Clear Health Errors)." },
	/* Device Identification Objects - all are optional */
	{ method_test_HID, "Test _HID (Hardware ID)." },
	{ NULL, NULL }
};

static int acpi_nvdimm_deinit(fwts_framework *fw)
{
	FWTS_UNUSED(fw);
	fwts_acpica_deinit();

	return FWTS_OK;
}

static fwts_framework_ops acpi_nvdimm_ops = {
	.description = "NVDIMM device test",
	.init        = acpi_nvdimm_init,
	.deinit      = acpi_nvdimm_deinit,
	.minor_tests = acpi_nvdimm_tests
};

FWTS_REGISTER("acpi_nvdimm", &acpi_nvdimm_ops, FWTS_TEST_ANYTIME, FWTS_FLAG_BATCH | FWTS_FLAG_TEST_ACPI)

#endif
