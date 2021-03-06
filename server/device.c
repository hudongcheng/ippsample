/*
 * Device support for sample IPP server implementation.
 *
 * Copyright 2010-2015 by Apple Inc.
 *
 * These coded instructions, statements, and computer programs are the
 * property of Apple Inc. and are protected by Federal copyright
 * law.  Distribution and use rights are outlined in the file "LICENSE.txt"
 * which should have been included with this file.  If this file is
 * file is missing or damaged, see the license at "http://www.cups.org/".
 *
 * This file is subject to the Apple OS-Developed Software exception.
 */

#include "ippserver.h"


/*
 * 'serverCreateDevice()' - Create an output device tracking object.
 */

server_device_t *			/* O - Device */
serverCreateDevice(
    server_client_t *client)		/* I - Client */
{
  server_device_t	*device;	/* Device */
  ipp_attribute_t	*uuid;		/* output-device-uuid */


  if ((uuid = ippFindAttribute(client->request, "output-device-uuid", IPP_TAG_URI)) == NULL)
    return (NULL);

  if ((device = calloc(1, sizeof(server_device_t))) == NULL)
    return (NULL);

  _cupsRWInit(&device->rwlock);

  device->uuid  = strdup(ippGetString(uuid, 0, NULL));
  device->state = IPP_PSTATE_STOPPED;

  _cupsRWLockWrite(&client->printer->rwlock);
  cupsArrayAdd(client->printer->devices, device);
  _cupsRWUnlock(&client->printer->rwlock);

  return (device);
}


/*
 * 'serverDeleteDevice()' - Remove a device from a printer.
 *
 * Note: Caller is responsible for locking the printer object.
 */

void
serverDeleteDevice(server_device_t *device)	/* I - Device */
{
 /*
  * Free memory used for the device...
  */

  _cupsRWDeinit(&device->rwlock);

  if (device->name)
    free(device->name);

  free(device->uuid);

  ippDelete(device->attrs);

  free(device);
}


/*
 * 'serverFindDevice()' - Find a device.
 */

server_device_t *			/* I - Device */
serverFindDevice(server_client_t *client)	/* I - Client */
{
  ipp_attribute_t	*uuid;		/* output-device-uuid */
  server_device_t	key,		/* Search key */
			*device;	/* Matching device */


  if ((uuid = ippFindAttribute(client->request, "output-device-uuid", IPP_TAG_URI)) == NULL)
    return (NULL);

  key.uuid = (char *)ippGetString(uuid, 0, NULL);

  _cupsRWLockRead(&client->printer->rwlock);
  device = (server_device_t *)cupsArrayFind(client->printer->devices, &key);
  _cupsRWUnlock(&client->printer->rwlock);

  return (device);
}


/*
 * 'serverUpdateDeviceAttributesNoLock()' - Update the composite device attributes.
 *
 * Note: Caller MUST lock the printer object for writing before using.
 */

void
serverUpdateDeviceAttributesNoLock(
    server_printer_t *printer)		/* I - Printer */
{
  server_device_t	*device;	/* Current device */
  ipp_t			*dev_attrs;	/* Device attributes */


 /* TODO: Support multiple output devices, icons, etc... */
  device    = (server_device_t *)cupsArrayFirst(printer->devices);
  dev_attrs = ippNew();

  if (device)
    serverCopyAttributes(dev_attrs, device->attrs, NULL, IPP_TAG_PRINTER, 0);

  ippDelete(printer->dev_attrs);
  printer->dev_attrs = dev_attrs;

  printer->config_time = time(NULL);
}


/*
 * 'serverUpdateDeviceStatusNoLock()' - Update the composite device state.
 *
 * Note: Caller MUST lock the printer object for writing before using.
 */

void
serverUpdateDeviceStateNoLock(
    server_printer_t *printer)		/* I - Printer */
{
  server_device_t	*device;	/* Current device */
  ipp_attribute_t	*attr;		/* Current attribute */


 /* TODO: Support multiple output devices, icons, etc... */
  device = (server_device_t *)cupsArrayFirst(printer->devices);

  if ((attr = ippFindAttribute(device->attrs, "printer-state", IPP_TAG_ENUM)) != NULL)
    printer->dev_state = (ipp_pstate_t)ippGetInteger(attr, 0);
  else
    printer->dev_state = IPP_PSTATE_STOPPED;

  if ((attr = ippFindAttribute(device->attrs, "printer-state-reasons", IPP_TAG_KEYWORD)) != NULL)
    printer->dev_reasons = serverGetPrinterStateReasonsBits(attr);
  else
    printer->dev_reasons = SERVER_PREASON_PAUSED;

  printer->state_time = time(NULL);
}
