import { Save } from '~/lib/icons/Save';
import { Button } from '../ui/button';

export default function SaveButton() {
    return <Button size="icon" variant="secondary" className='p-3'>
        <Save className='text-foreground' />
    </Button>
}