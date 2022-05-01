package com.yen.dlna.xml;


import com.yen.dlna.DLNAManager;

import org.fourthline.cling.binding.staging.MutableService;
import org.fourthline.cling.binding.xml.DescriptorBindingException;
import org.fourthline.cling.binding.xml.UDA10ServiceDescriptorBinderSAXImpl;
import org.fourthline.cling.model.ValidationException;
import org.fourthline.cling.model.meta.Action;
import org.fourthline.cling.model.meta.Service;
import org.fourthline.cling.model.types.ServiceType;
import org.seamless.xml.SAXParser;
import org.xml.sax.InputSource;

import java.io.StringReader;


public class DLNAUDA10ServiceDescriptorBinderSAXImpl extends UDA10ServiceDescriptorBinderSAXImpl {

    private static final String TAG = DLNAUDA10ServiceDescriptorBinderSAXImpl.class.getSimpleName();

    @Override
    public <S extends Service> S describe(S undescribedService, String descriptorXml) throws DescriptorBindingException, ValidationException {

        if (descriptorXml == null || descriptorXml.length() == 0) {
            throw new DescriptorBindingException("Null or empty descriptor");
        }

        try {
            DLNAManager.logD(TAG, "Reading service from XML descriptor, content : \n");

            //     DLNAManager.logD(TAG, "==============================================\n");
      //     DLNAManager.logD(TAG, "Reading service from XML descriptor, content : \n" + descriptorXml);

            SAXParser parser = new DLNASAXParser();

            MutableService descriptor = new MutableService();

            hydrateBasic(descriptor, undescribedService);

            new RootHandler(descriptor, parser);

            parser.parse(
                    new InputSource(
                            // TODO: UPNP VIOLATION: Virgin Media Superhub sends trailing spaces/newlines after last XML element, need to trim()
                            new StringReader(descriptorXml.trim())
                    )
            );
            Service x = descriptor.build(undescribedService.getDevice());
            Action y[] = x.getActions();
            ServiceType z = x.getServiceType();
          //  DLNAManager.logD(TAG, "SS: "+     x.toString() );
           // x.getDevice().getDisplayString();
           // DLNAManager.logD(TAG, "Type: "+  z.getType()  );

            for(Action a : y){
                DLNAManager.logD(TAG, a.getName());
            }
            // Build the immutable descriptor graph
            return  (S)x;

        } catch (ValidationException ex) {
            throw ex;
        } catch (Exception ex) {
            throw new DescriptorBindingException("Could not parse service descriptor: " + ex.toString(), ex);
        }
    }
}
