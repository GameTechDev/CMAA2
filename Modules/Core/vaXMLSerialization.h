#pragma once

#include "vaCore.h"

#include "IntegratedExternals\vaTinyxml2Integration.h"

namespace VertexAsylum
{
    class vaXMLSerializer;

    class vaXMLSerializable
    {
    protected:
        vaXMLSerializable( ) { };
        virtual ~vaXMLSerializable( ) { };

    public:
        virtual bool                                Serialize( vaXMLSerializer & serializer ) = 0;
    };

    class vaXMLSerializer
    {
    private:
        tinyxml2::XMLPrinter        m_writePrinter;
        vector<string>              m_writeElementNameStack;
        int                         m_writeElementStackDepth;

        tinyxml2::XMLDocument       m_readDocument;
        tinyxml2::XMLElement *      m_currentReadElement;

        bool                        m_isReading;
        bool                        m_isWriting;

    public:
        // parse data, set to loading mode
        vaXMLSerializer( const char * inputData, size_t dataSize )
        {
            m_isWriting = false;
            m_isReading = m_readDocument.Parse( inputData, dataSize ) == tinyxml2::XMLError::XML_SUCCESS;
            m_currentReadElement = nullptr;
            m_writeElementStackDepth = 0;
        }
        // parse data, set to loading mode
        explicit vaXMLSerializer( vaFileStream & fileStream )
        {
            m_isWriting = false;
            m_isReading = false;
            m_currentReadElement = nullptr;
            m_writeElementStackDepth = 0;

            int64 fileLength = fileStream.GetLength( );
            if( fileLength == 0 )
                return;

            if( fileStream.GetPosition() != 0 )
                fileStream.Seek( 0 );

            char * buffer = new char[ fileLength+1 ];
            if( fileStream.Read( buffer, fileLength ) )
            {
                buffer[fileLength] = 0;
                m_isReading = m_readDocument.Parse( (const char*)buffer, fileLength ) == tinyxml2::XMLError::XML_SUCCESS;
                assert( m_isReading ); // error parsing?
            }
            else
            {
                assert( false ); // error reading?
            }
            delete[] buffer;
        }

        // open printer, set to storing mode
        vaXMLSerializer( ) : m_writePrinter( nullptr, false, 0 )
        {
            m_isWriting = true;
            m_isReading = false;
            m_currentReadElement = nullptr;
            m_writeElementStackDepth = 0;
        }

        ~vaXMLSerializer( )
        {
            assert( m_currentReadElement == nullptr );      // forgot to ReadPopToParentElement?
            assert( m_writeElementNameStack.size() == 0 );  // forgot to WriteCloseElement?
            assert( m_writeElementStackDepth == 0 );        // forgot to WriteCloseElement?
        }

        bool                        IsReading( ) const { return m_isReading; }
        bool                        IsWriting( ) const { return m_isWriting; }

        tinyxml2::XMLPrinter &      GetWritePrinter( ) { assert( m_isWriting ); return m_writePrinter; }

        tinyxml2::XMLDocument &     GetReadDocument( ) { assert( m_isReading ); return m_readDocument; }

        tinyxml2::XMLElement *      GetCurrentReadElement( );

        bool                        ReaderAdvanceToChildElement( const char * name );
        bool                        ReaderAdvanceToSiblingElement( const char * name );
        bool                        ReaderPopToParentElement( const char * nameToVerify );

        bool                        ReadBoolAttribute( const char * name, bool & outVal )     const;
        bool                        ReadInt32Attribute( const char * name, int32 & outVal )   const;
        bool                        ReadUInt32Attribute( const char * name, uint32 & outVal ) const;
        bool                        ReadInt64Attribute( const char * name, int64 & outVal )   const;
        bool                        ReadFloatAttribute( const char * name, float & outVal )   const;
        bool                        ReadDoubleAttribute( const char * name, double & outVal ) const;
        bool                        ReadStringAttribute( const char * name, string & outVal ) const;

        bool                        ReadAttribute( const char * name, bool & outVal )       const { return ReadBoolAttribute( name, outVal ); }
        bool                        ReadAttribute( const char * name, int32 & outVal )      const { return ReadInt32Attribute( name, outVal ); }
        bool                        ReadAttribute( const char * name, uint32 & outVal )     const { return ReadUInt32Attribute( name, outVal ); }
        bool                        ReadAttribute( const char * name, int64 & outVal )      const { return ReadInt64Attribute( name, outVal ); }
        bool                        ReadAttribute( const char * name, float & outVal )      const { return ReadFloatAttribute( name, outVal ); }
        bool                        ReadAttribute( const char * name, double & outVal )     const { return ReadDoubleAttribute( name, outVal ); }
        bool                        ReadAttribute( const char * name, string & outVal )     const { return ReadStringAttribute( name, outVal ); }
        bool                        ReadAttribute( const char * name, vaGUID & val )        const;
        bool                        ReadAttribute( const char * name, vaVector3 & val )     const;
        bool                        ReadAttribute( const char * name, vaVector4 & val )     const;


        bool                        WriterOpenElement( const char * name );
        bool                        WriterCloseElement( const char * nameToVerify );

        bool                        WriteAttribute( const char * name, bool  outVal ) { assert( m_isWriting ); if( !m_isWriting ) return false; m_writePrinter.PushAttribute( name, outVal ); return true; }
        bool                        WriteAttribute( const char * name, int32  outVal ) { assert( m_isWriting ); if( !m_isWriting ) return false; m_writePrinter.PushAttribute( name, outVal ); return true; }
        bool                        WriteAttribute( const char * name, uint32  outVal ) { assert( m_isWriting ); if( !m_isWriting ) return false; m_writePrinter.PushAttribute( name, outVal ); return true; }
        bool                        WriteAttribute( const char * name, int64  outVal ) { assert( m_isWriting ); if( !m_isWriting ) return false; m_writePrinter.PushAttribute( name, outVal ); return true; }
        bool                        WriteAttribute( const char * name, float  outVal ) { assert( m_isWriting ); if( !m_isWriting ) return false; m_writePrinter.PushAttribute( name, outVal ); return true; }
        bool                        WriteAttribute( const char * name, double  outVal ) { assert( m_isWriting ); if( !m_isWriting ) return false; m_writePrinter.PushAttribute( name, outVal ); return true; }
        bool                        WriteAttribute( const char * name, const string & outVal ) { assert( m_isWriting ); if( !m_isWriting ) return false; m_writePrinter.PushAttribute( name, outVal.c_str( ) ); return true; }
        bool                        WriteAttribute( const char * name, vaGUID & val );
        bool                        WriteAttribute( const char * name, vaVector3 & val );
        bool                        WriteAttribute( const char * name, vaVector4 & val );

        bool                        WriterSaveToFile( vaFileStream & fileStream );

        // Serialization helpers - both reader & writer
        bool                        SerializeOpenChildElement( const char * name );
        bool                        SerializePopToParentElement( const char * nameToVerify );
        
        // Simple ("value") types use attributes for serialization because they take up less space
        bool                        SerializeValue( const char * name, bool & val ) { if( m_isWriting ) return WriteAttribute( name, val ); if( m_isReading ) return ReadAttribute( name, val ); assert( false ); return false; }
        bool                        SerializeValue( const char * name, int32 & val ) { if( m_isWriting ) return WriteAttribute( name, val ); if( m_isReading ) return ReadAttribute( name, val ); assert( false ); return false; }
        bool                        SerializeValue( const char * name, uint32 & val ) { if( m_isWriting ) return WriteAttribute( name, val ); if( m_isReading ) return ReadAttribute( name, val ); assert( false ); return false; }
        bool                        SerializeValue( const char * name, int64 & val ) { if( m_isWriting ) return WriteAttribute( name, val ); if( m_isReading ) return ReadAttribute( name, val ); assert( false ); return false; }
        bool                        SerializeValue( const char * name, float & val ) { if( m_isWriting ) return WriteAttribute( name, val ); if( m_isReading ) return ReadAttribute( name, val ); assert( false ); return false; }
        bool                        SerializeValue( const char * name, double & val ) { if( m_isWriting ) return WriteAttribute( name, val ); if( m_isReading ) return ReadAttribute( name, val ); assert( false ); return false; }
        bool                        SerializeValue( const char * name, string & val ) { if( m_isWriting ) return WriteAttribute( name, val ); if( m_isReading ) return ReadAttribute( name, val ); assert( false ); return false; }
        bool                        SerializeValue( const char * name, vaGUID & val ) { if( m_isWriting ) return WriteAttribute( name, val ); if( m_isReading ) return ReadAttribute( name, val ); assert( false ); return false; }
        bool                        SerializeValue( const char * name, vaVector3 & val ) { if( m_isWriting ) return WriteAttribute( name, val ); if( m_isReading ) return ReadAttribute( name, val ); assert( false ); return false; }
        bool                        SerializeValue( const char * name, vaVector4 & val ) { if( m_isWriting ) return WriteAttribute( name, val ); if( m_isReading ) return ReadAttribute( name, val ); assert( false ); return false; }

        // A version of SerializeValue that will simply, if reading, set default if value is missing and always returns true
        template< typename ValueType >
        bool                        SerializeValue( const char * name, ValueType & val, const ValueType & defaultVal );
        
        // support for vaXMLSerializable objects
        template< typename ElementType >
        bool                        SerializeObjectVector( const char * name, vector< shared_ptr<ElementType> > & objects );
        // support for vaXMLSerializable objects
        template< typename ElementType >
        bool                        SerializeObjectVector( const char * name, vector< ElementType > & objects );
        
        // support for native value types (bool, int32, float, string, vaGUID, etc., see above)
        template< typename ElementType >
        bool                        SerializeValueVector( const char * name, vector< ElementType > & elements );
    };


    inline tinyxml2::XMLElement * vaXMLSerializer::GetCurrentReadElement( ) { return m_currentReadElement; }
    inline bool                   vaXMLSerializer::ReaderAdvanceToChildElement( const char * name )
    {
        assert( m_isReading ); if( !m_isReading ) return false;
        tinyxml2::XMLElement * e = ( m_currentReadElement != nullptr ) ? ( m_currentReadElement->FirstChildElement( name ) ) : ( m_readDocument.FirstChildElement( name ) );
        if( e != nullptr ) { m_currentReadElement = e; return true; } return false;
    }
    inline bool                   vaXMLSerializer::ReaderAdvanceToSiblingElement( const char * name )
    {
        assert( m_isReading ); if( !m_isReading ) return false;
        tinyxml2::XMLElement * e = ( m_currentReadElement != nullptr ) ? ( m_currentReadElement->NextSiblingElement( name ) ) : ( m_readDocument.NextSiblingElement( name ) );
        if( e != nullptr )
        {
            m_currentReadElement = e; return true;
        }
        return false;
    }
    inline bool                   vaXMLSerializer::ReaderPopToParentElement( const char * nameToVerify )
    {
        assert( m_isReading ); if( !m_isReading ) return false;
        if( nameToVerify != nullptr )
        {
            assert( m_currentReadElement != nullptr && strncmp( m_currentReadElement->Name(), nameToVerify, 32768 ) == 0 );
        }

        if( m_currentReadElement == nullptr )
            return false;
        tinyxml2::XMLElement * e = ( m_currentReadElement->Parent( ) != nullptr ) ? ( m_currentReadElement->Parent( )->ToElement( ) ) : ( nullptr );
        m_currentReadElement = e;
        return true;
    }
    inline bool                   vaXMLSerializer::ReadBoolAttribute( const char * name, bool & outVal )   const
    {
        assert( m_isReading ); if( !m_isReading ) return false;
        if( m_currentReadElement == nullptr )   return false;
        return m_currentReadElement->QueryBoolAttribute( name, &outVal ) == tinyxml2::XML_SUCCESS;
    }
    inline bool                   vaXMLSerializer::ReadInt32Attribute( const char * name, int32 & outVal )   const
    {
        assert( m_isReading ); if( !m_isReading ) return false;
        if( m_currentReadElement == nullptr )   return false;
        return m_currentReadElement->QueryIntAttribute( name, &outVal ) == tinyxml2::XML_SUCCESS;
    }
    inline bool                   vaXMLSerializer::ReadUInt32Attribute( const char * name, uint32 & outVal ) const
    {
        assert( m_isReading ); if( !m_isReading ) return false;
        if( m_currentReadElement == nullptr )   return false;
        return m_currentReadElement->QueryUnsignedAttribute( name, &outVal ) == tinyxml2::XML_SUCCESS;
    }
    inline bool                   vaXMLSerializer::ReadInt64Attribute( const char * name, int64 & outVal )   const
    {
        assert( m_isReading ); if( !m_isReading ) return false;
        if( m_currentReadElement == nullptr )   return false;
        return m_currentReadElement->QueryInt64Attribute( name, &outVal ) == tinyxml2::XML_SUCCESS;
    }
    inline bool                   vaXMLSerializer::ReadFloatAttribute( const char * name, float & outVal )   const
    {
        assert( m_isReading ); if( !m_isReading ) return false;
        if( m_currentReadElement == nullptr )   return false;
        return m_currentReadElement->QueryFloatAttribute( name, &outVal ) == tinyxml2::XML_SUCCESS;
    }
    inline bool                   vaXMLSerializer::ReadDoubleAttribute( const char * name, double & outVal ) const
    {
        assert( m_isReading ); if( !m_isReading ) return false;
        if( m_currentReadElement == nullptr )   return false;
        return m_currentReadElement->QueryDoubleAttribute( name, &outVal ) == tinyxml2::XML_SUCCESS;
    }
    inline bool                   vaXMLSerializer::ReadStringAttribute( const char * name, string & outVal ) const
    {
        assert( m_isReading ); if( !m_isReading ) return false;
        if( m_currentReadElement == nullptr )   return false;
        const tinyxml2::XMLAttribute * a = ( ( const tinyxml2::XMLElement * )m_currentReadElement )->FindAttribute( name );
        if( a != nullptr )
        {
            outVal = a->Value( );
            return true;
        }
        return false;
    }

    inline bool                 vaXMLSerializer::WriterOpenElement( const char * name )
    {
        assert( m_isWriting ); if( !m_isWriting ) return false;
        m_writePrinter.OpenElement( name );
        m_writeElementStackDepth++;
        m_writeElementNameStack.push_back( name );
        return true;
    }

    inline bool                 vaXMLSerializer::WriterCloseElement( const char * nameToVerify )
    {
        if( nameToVerify != nullptr )
        {
            assert( m_writeElementNameStack.size() > 0 && m_writeElementNameStack.back() == nameToVerify );
        }

        assert( m_isWriting ); if( !m_isWriting ) return false;
        assert( m_writeElementStackDepth > 0 ); if( m_writeElementStackDepth <= 0 ) return false;
        m_writePrinter.CloseElement( );
        m_writeElementNameStack.pop_back();
        m_writeElementStackDepth--;
        return true;
    }

    inline bool                 vaXMLSerializer::WriterSaveToFile( vaFileStream & fileStream )
    {
        assert( m_isWriting ); if( !m_isWriting ) return false;
        fileStream.Seek( 0 );
        fileStream.Truncate( );
        fileStream.Write( GetWritePrinter( ).CStr( ), GetWritePrinter( ).CStrSize( ) - 1 );
        fileStream.Flush( );
        return true;
    }

    inline bool                 vaXMLSerializer::ReadAttribute( const char * name, vaGUID & val ) const
    {
        string uiid;
        if( !ReadStringAttribute( name, uiid ) )
            { assert(false); return false; }

        val = vaCore::GUIDFromStringA( uiid );
        return true;
    }

    inline bool                 vaXMLSerializer::WriteAttribute( const char * name, vaGUID & val )
    {
        return WriteAttribute( name, vaCore::GUIDToStringA( val ) );
    }

    inline bool                 vaXMLSerializer::ReadAttribute( const char * name, vaVector3 & val )     const
    {
        string valStr;
        if( !ReadStringAttribute( name, valStr ) )
            { assert(false); return false; }

        return vaVector3::FromString( valStr, val );
    }
    inline bool                 vaXMLSerializer::WriteAttribute( const char * name, vaVector3 & val )
    {
        return WriteAttribute( name, vaVector3::ToString( val ) );
    }

    inline bool                 vaXMLSerializer::ReadAttribute( const char * name, vaVector4 & val )     const
    {
        string valStr;
        if( !ReadStringAttribute( name, valStr ) )
            { assert(false); return false; }

        return vaVector4::FromString( valStr, val );
    }
    inline bool                 vaXMLSerializer::WriteAttribute( const char * name, vaVector4 & val )
    {
        return WriteAttribute( name, vaVector4::ToString( val ) );
    }

    inline bool                 vaXMLSerializer::SerializeOpenChildElement( const char * name ) 
    { 
        if( m_isWriting ) 
            return WriterOpenElement( name );   
        if( m_isReading ) 
            return ReaderAdvanceToChildElement( name );
        assert( false ); 
        return false; 
    }

    template< typename ValueType >
    inline bool                 vaXMLSerializer::SerializeValue( const char * name, ValueType & val, const ValueType & defaultVal ) 
    { 
        if( m_isWriting ) return SerializeValue( name, val ); 
        if( m_isReading ) 
        {
            bool ret = ReadAttribute( name, val );
            if( !ret )
                val = defaultVal;
            return true;    // always return true
        }
        assert( false );
        return false;
    }

    inline bool                 vaXMLSerializer::SerializePopToParentElement( const char * nameToVerify ) 
    { 
        if( m_isWriting ) 
        {
            return WriterCloseElement( nameToVerify );
        }
        if( m_isReading ) 
        {   
            return ReaderPopToParentElement( nameToVerify ); 
        }
        assert( false ); 
        return false; 
    }

    template< typename ElementType >
    inline bool                 vaXMLSerializer::SerializeObjectVector( const char * name, vector< shared_ptr<ElementType> > & objects )
    {
        if( !SerializeOpenChildElement( name ) )
            { assert(false); return false; }

        uint32 count = (uint32)objects.size();
        if( !SerializeValue( "count", count ) )
            { assert(false); return false; }
        if( m_isReading )
            objects.resize( count );
        string elementName;
        for( uint32 i = 0; i < count; i++ )
        {
            elementName = vaStringTools::Format( "a%d", i );
            if( !SerializeOpenChildElement( elementName.c_str() ) )
                { assert(false); return false; }
            if( m_isReading )
                objects[i] = std::make_shared<ElementType>( );
            objects[i]->Serialize( *this );
            if( !SerializePopToParentElement( elementName.c_str() ) )
                { assert(false); return false; }
        }

        if( !SerializePopToParentElement( name ) )
            { assert(false); return false; }

        return true;
    }

    template< typename ElementType >
    inline bool                 vaXMLSerializer::SerializeObjectVector( const char * name, vector< ElementType > & objects )
    {
        if( !SerializeOpenChildElement( name ) )
            { assert(false); return false; }

        uint32 count = (uint32)objects.size();
        if( !SerializeValue( "count", count ) )
            { assert(false); return false; }
        if( m_isReading )
            objects.resize( count );
        string elementName;
        for( uint32 i = 0; i < count; i++ )
        {
            elementName = vaStringTools::Format( "a%d", i );
            if( !SerializeOpenChildElement( elementName.c_str() ) )
                { assert(false); return false; }
            objects[i].Serialize( *this );
            if( !SerializePopToParentElement( elementName.c_str() ) )
                { assert(false); return false; }
        }

        if( !SerializePopToParentElement( name ) )
            { assert(false); return false; }

        return true;
    }

    template< typename ElementType >
    inline bool                 vaXMLSerializer::SerializeValueVector( const char * name, vector< ElementType > & elements )
    {
        if( !SerializeOpenChildElement( name ) )
            { assert(false); return false; }

        uint32 count = (uint32)elements.size();
        if( !SerializeValue( "count", count ) )
            { assert(false); return false; }
        if( m_isReading )
            elements.resize( count );
        for( uint32 i = 0; i < count; i++ )
        {
            if( !SerializeValue( vaStringTools::Format( "a%d", i ).c_str(), elements[i] ) )
                { assert(false); return false; }
        }

        if( !SerializePopToParentElement( name ) )
            { assert(false); return false; }

        return true;
    }

}